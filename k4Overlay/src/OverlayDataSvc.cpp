#include "OverlayDataSvc.h"
#include "k4FWCore/PodioDataSvc.h"
#include <GaudiKernel/StatusCode.h>
#include "GaudiKernel/IConversionSvc.h"
#include "GaudiKernel/IEventProcessor.h"
#include "GaudiKernel/IProperty.h"
#include "GaudiKernel/ISvcLocator.h"
#include "k4FWCore/DataWrapper.h"

#define MCPARTCOLLPRIO 1
#define CONTRIBCOLLPRIO 2
#define CALOHITCOLLPRIO 3
#define TRACKHITCOLLPRIO 4

using MCPartColl = edm4hep::MCParticleCollection;
using CaloContribColl = edm4hep::CaloHitContributionCollection;
using CaloHitColl = edm4hep::SimCalorimeterHitCollection;
using TrackHitColl = edm4hep::SimTrackerHitCollection;

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
     * Collection priority table
     * ***********************************************************************/
    for (string item : coll_defs)
    {
        auto idx = item.find("=");
        if (idx == item.npos) continue;
        string coll_name = item.substr(0, idx);
        string coll_type = item.substr(idx + 1, item.size());

        if (coll_type == "edm4hep::SimTrackerHit")
        {
            collname_set.emplace(TRACKHITCOLLPRIO, coll_name);
        }
        else if (coll_type == "edm4hep::SimCalorimeterHit")
        {
            collname_set.emplace(CALOHITCOLLPRIO, coll_name);
        }
        else if (coll_type == "edm4hep::MCParticle")
        {
            collname_set.emplace(MCPARTCOLLPRIO, coll_name);
        }
        else if (coll_type == "edm4hep::CaloHitContribution")
        {
            collname_set.emplace(CONTRIBCOLLPRIO, coll_name);
        }
    }

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
    for (auto [priority, collname] : collname_set)
    {
        if (priority == MCPARTCOLLPRIO)
            mc_coll_map.emplace(collname, MCPartColl());
        else if (priority == CONTRIBCOLLPRIO)
            cc_coll_map.emplace(collname, CaloContribColl());
        else if (priority == CALOHITCOLLPRIO)
            sc_coll_map.emplace(collname, CaloHitColl());
        else if (priority == TRACKHITCOLLPRIO)
            st_coll_map.emplace(collname, TrackHitColl());
    }

    if (!m_reading_from_file) return StatusCode::SUCCESS;

    if (auto st = mergeFrame(podio::Frame(m_reader.readEntry("events", m_eventNum + m_1stEvtEntry)));
        st != StatusCode::SUCCESS)
    {
        error() << "Error merging signal frame" << endmsg;
        return StatusCode::FAILURE;
    }

    for (int k = 0; k < num_bib; k++)
    {
        if (auto bib1 = m_BIB1Svc->getEventFrame(); !bib1)
        {
            error() << "Wrong frame for BIB1" << endmsg;
            return StatusCode::FAILURE;
        }
        else if (auto st = mergeFrame(bib1.value()); st != StatusCode::SUCCESS)
        {
            error() << "Error merging BIB1 frame" << endmsg;
            return StatusCode::FAILURE;
        }

        if (auto bib2 = m_BIB2Svc->getEventFrame(); !bib2)
        {
            error() << "Wrong frame for BIB2" << endmsg;
            return StatusCode::FAILURE;
        }
        else if (auto st = mergeFrame(bib2.value()); st != StatusCode::SUCCESS)
        {
            error() << "Error merging BIB2 frame" << endmsg;
            return StatusCode::FAILURE;
        }

        if (auto ipp = m_IPairSvc->getEventFrame(); !ipp)
        {
            error() << "Wrong frame for IPP" << endmsg;
            return StatusCode::FAILURE;
        }
        else if (auto st = mergeFrame(ipp.value()); st != StatusCode::SUCCESS)
        {
            error() << "Error merging IPP frame" << endmsg;
            return StatusCode::FAILURE;
        }
    }

    m_eventframe = podio::Frame();
    for (auto [priority, collname] : collname_set)
    {
        if (priority == MCPARTCOLLPRIO)
            m_eventframe.put<MCPartColl>(std::move(mc_coll_map[collname]), collname);
        else if (priority == CONTRIBCOLLPRIO)
            m_eventframe.put<CaloContribColl>(std::move(cc_coll_map[collname]), collname);
        else if (priority == CALOHITCOLLPRIO)
            m_eventframe.put<CaloHitColl>(std::move(sc_coll_map[collname]), collname);
        else if (priority == TRACKHITCOLLPRIO)
            m_eventframe.put<TrackHitColl>(std::move(st_coll_map[collname]), collname);
    }
    mc_coll_map.clear();
    cc_coll_map.clear();
    sc_coll_map.clear();
    st_coll_map.clear();

    return StatusCode::SUCCESS;
}

StatusCode OverlayDataSvc::mergeFrame(const podio::Frame& frame)
{
    for (auto [priority, collname] : collname_set)
    {
        if (priority == MCPARTCOLLPRIO)
        {
            const MCPartColl* b_coll = static_cast<const MCPartColl*>(frame.get(collname));
            for (auto mc_item : *b_coll)
            {
                edm4hep::MutableMCParticle mc_part {
                    mc_item.getPDG(), mc_item.getGeneratorStatus(),
                    mc_item.getSimulatorStatus(), mc_item.getCharge(), mc_item.getTime(),
                    mc_item.getMass(), mc_item.getVertex(), mc_item.getEndpoint(),
                    mc_item.getMomentum(), mc_item.getMomentumAtEndpoint(),
                    mc_item.getSpin(), mc_item.getColorFlow()
                };
                mc_coll_map[collname].push_back(mc_part);
            }
        }
        else if (priority == CONTRIBCOLLPRIO)
        {
            const CaloContribColl* b_coll = static_cast<const CaloContribColl*>(frame.get(collname));
            for (auto cc_item : *b_coll)
            {
                edm4hep::MutableCaloHitContribution contrib {
                    cc_item.getPDG(), cc_item.getEnergy(),
                    cc_item.getTime(), cc_item.getStepPosition()
                };
                cc_coll_map[collname].push_back(contrib);
            }
        }
        else if (priority == CALOHITCOLLPRIO)
        {
            const CaloHitColl* b_coll = static_cast<const CaloHitColl*>(frame.get(collname));
            for (auto sc_item : *b_coll)
            {
                edm4hep::MutableSimCalorimeterHit sc_hit {
                    sc_item.getCellID(), sc_item.getEnergy(), sc_item.getPosition()
                };
                sc_coll_map[collname].push_back(sc_hit);
            }
        }
        else if (priority == TRACKHITCOLLPRIO)
        {
            const TrackHitColl* b_coll = static_cast<const TrackHitColl*>(frame.get(collname));
            for (auto st_item : *b_coll)
            {
                edm4hep::MutableSimTrackerHit st_hit {
                    st_item.getCellID(), st_item.getEDep(), st_item.getTime(),
                    st_item.getPathLength(), st_item.getQuality(),
                    st_item.getPosition(), st_item.getMomentum()
                };
                st_coll_map[collname].push_back(st_hit);
            }
        }
    }
    return StatusCode::SUCCESS;
}
