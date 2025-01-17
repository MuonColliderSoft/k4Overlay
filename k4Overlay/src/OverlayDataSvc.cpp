#include "OverlayDataSvc.h"
#include "k4FWCore/PodioDataSvc.h"
#include <GaudiKernel/StatusCode.h>
#include "GaudiKernel/IConversionSvc.h"
#include "GaudiKernel/IEventProcessor.h"
#include "GaudiKernel/IProperty.h"
#include "GaudiKernel/ISvcLocator.h"
#include "k4FWCore/DataWrapper.h"

#include "podio/CollectionBase.h"

#include <random>

DECLARE_COMPONENT(OverlayDataSvc)

StatusCode OverlayDataSvc::initialize()
{
    if (auto st = DataSvc::initialize(); st != StatusCode::SUCCESS)
    {
        error() << "Service initialization failure" << endmsg;
        return st;
    }

    ISvcLocator *svc_loc = serviceLocator();
    m_cnvSvc = svc_loc->service("EventPersistencySvc");
    if (auto st = setDataLoader(m_cnvSvc); st != StatusCode::SUCCESS)
    {
        error() << "Error attaching data loader facility" << endmsg;
        return st;
    }

    /* ************************************************************************
     * Signal initialization
     * ***********************************************************************/
    if (m_filenames.size() > 0 and m_filenames[0] != "")
    {
        m_reading_from_file = true;
        m_reader.openFiles(m_filenames);
        m_numAvailableEvents = m_reader.getEntries("events");
        m_numAvailableEvents -= m_1stEvtEntry;
    }

    if (m_reading_from_file)
    {
        if (auto metadata = m_reader.readEntry("metadata", 0))
        {
            m_metadataframe = std::move(metadata);
        }
        else
        {
            warning() << "Reading file without a 'metadata' category." << endmsg;
            m_metadataframe = podio::Frame();
        }
    }
    else
    {
        m_metadataframe = podio::Frame();
    }

    IProperty *property;
    auto sc = service("ApplicationMgr", property);
    if (sc == StatusCode::FAILURE)
    {
        error() << "Could not get ApplicationMgr properties" << std::endl;
        return sc;
    }
    Gaudi::Property<int> evtMax;
    evtMax.assign(property->getProperty("EvtMax"));
    m_requestedEventMax = evtMax - m_1stEvtEntry;

    // if run with a fixed number of requested events and we have enough
    // in the file we don't need to check if we run out of events
    if (m_requestedEventMax > 0 && m_requestedEventMax <= m_numAvailableEvents)
    {
        m_bounds_check_needed = false;
    }

    /* ************************************************************************
     * Background initialization
     * ***********************************************************************/
    std::random_device r_device;
    std::mt19937 r_generator(r_device());
    std::vector<std::string> s_filenames;

    for (auto item : b_filenames) s_filenames.push_back(item);
    if (s_filenames.size() > 1) std::shuffle(s_filenames.begin(), s_filenames.end(), r_generator);
    if (s_filenames.empty())
    {
        error() << "No file names specified" << endmsg;
        return StatusCode::FAILURE;
    }

    b_reader.openFiles(s_filenames);
    if ((total_bevns = b_reader.getEntries("events")) == 0)
    {
        error() << "No events found" << endmsg;
        return StatusCode::FAILURE;
    }

    std::uniform_int_distribution<unsigned> uni_distro { 0, total_bevns - 1 };
    curr_bevn = uni_distro(r_generator);

    return StatusCode::SUCCESS;
}

StatusCode OverlayDataSvc::reinitialize()
{
    return StatusCode::SUCCESS;
}

StatusCode OverlayDataSvc::finalize()
{
    m_cnvSvc = 0;  // release
    DataSvc::finalize().ignore();
    return StatusCode::SUCCESS;
}

StatusCode OverlayDataSvc::clearStore()
{
    // as the frame takes care of the ownership of the podio::Collections,
    // make sure the DataWrappers don't cause a double delete
    for (auto wrapper : m_podio_datawrappers)
    {
        wrapper->resetData();
    }
    m_podio_datawrappers.clear();

    DataSvc::clearStore().ignore();
    return StatusCode::SUCCESS;
}

StatusCode OverlayDataSvc::i_setRoot(std::string root_path,  IOpaqueAddress *pRootAddr)
{
    auto res = readFrames();
    if (res != StatusCode::SUCCESS) return res;
    return DataSvc::i_setRoot(root_path, pRootAddr);
}

StatusCode OverlayDataSvc::i_setRoot(std::string root_path, DataObject *pRootObj)
{
    auto res = readFrames();
    if (res != StatusCode::SUCCESS) return res;
    return DataSvc::i_setRoot(root_path, pRootObj);
}

void OverlayDataSvc::endOfRead()
{
    m_eventNum++;
    if (!m_bounds_check_needed) return;

    StatusCode sc;
    // m_eventNum already points to the next event here so check if it is available
    if (m_eventNum >= m_numAvailableEvents)
    {
        info() << "Reached end of file with event " << m_eventNum << " ("
                << m_requestedEventMax << " events requested)" << endmsg;
        IEventProcessor *eventProcessor;
        sc = service("ApplicationMgr", eventProcessor);
        sc = eventProcessor->stopRun();
    }
}

const std::string_view OverlayDataSvc::getCollectionType(const std::string &collName)
{
    const auto coll = m_eventframe.get(collName);
    if (coll == nullptr)
    {
        error() << "Collection " << collName << " does not exist." << endmsg;
        return "";
    }
    return coll->getTypeName();
}

StatusCode OverlayDataSvc::registerObject(std::string_view parentPath,
        std::string_view fullPath, DataObject *pObject)
{
    DataWrapperBase *wrapper = dynamic_cast<DataWrapperBase*>(pObject);
    if (wrapper != nullptr)
    {
        podio::CollectionBase *coll = wrapper->collectionBase();
        if (coll != nullptr)
        {
            size_t pos = fullPath.find_last_of("/");
            std::string shortPath(fullPath.substr(pos + 1, fullPath.length()));
            // Attention: this passes the ownership of the data to the frame
            m_eventframe.put(std::unique_ptr < podio::CollectionBase > (coll), shortPath);
            m_podio_datawrappers.push_back(wrapper);
        }
    }
    return DataSvc::registerObject(parentPath, fullPath, pObject);
}

StatusCode OverlayDataSvc::readFrames()
{
    m_eventframe = podio::Frame();
    if (!m_reading_from_file) return StatusCode::SUCCESS;


    auto st = mergeFrame(podio::Frame(m_reader.readEntry("events", m_eventNum + m_1stEvtEntry)));

    for (int k = 0; k < num_bib; k++)
    {
        st = mergeFrame(podio::Frame(b_reader.readEntry("events", curr_bevn)));

        curr_bevn++;
        if (curr_bevn == total_bevns) curr_bevn = 0;
    }

    return StatusCode::SUCCESS;
}

StatusCode OverlayDataSvc::mergeFrame(const podio::Frame& frame)
{
    // TODO m_eventframe += frame
    return StatusCode::SUCCESS;
}
