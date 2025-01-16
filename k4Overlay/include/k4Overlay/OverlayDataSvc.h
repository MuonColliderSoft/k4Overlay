#ifndef OVERLAYDATASVC_H
#define OVERLAYDATASVC_H

#include "GaudiKernel/DataSvc.h"
#include "GaudiKernel/IConversionSvc.h"
// PODIO
#include <utility>
#include "podio/CollectionBase.h"
#include "podio/CollectionIDTable.h"
#include "podio/Frame.h"
#include "podio/ROOTFrameReader.h"
// Forward declarations
#include "k4FWCore/DataWrapper.h"
class DataWrapperBase;
class PodioOutput;
template<typename T> class MetaDataHandle;

class OverlayDataSvc: public DataSvc
{
    template<typename T> friend class MetaDataHandle;
    friend class PodioOutput;

public:
    typedef std::vector<std::pair<std::string, podio::CollectionBase*>> CollRegistry;

    StatusCode initialize() final;
    StatusCode reinitialize() final;
    StatusCode finalize() final;
    StatusCode clearStore() final;
    StatusCode i_setRoot(std::string root_path, IOpaqueAddress *pRootAddr) final;
    StatusCode i_setRoot(std::string root_path, DataObject *pRootObj) final;

    OverlayDataSvc(const std::string &name, ISvcLocator *svc);
    virtual ~OverlayDataSvc() {}

    // Use DataSvc functionality except where we override
    using DataSvc::registerObject;
    /// Overriding standard behaviour of evt service
    /// Register object with the data store.
    virtual StatusCode registerObject(std::string_view parentPath,
            std::string_view fullPath, DataObject *pObject) override final;

    const std::string_view getCollectionType(const std::string &collName);

    template<typename T> StatusCode readCollection(const std::string &collName)
    {
        DataObject *objectPtr = nullptr;
        if (DataSvc::findObject("/Event", "/" + collName, objectPtr))
        {
            debug() << "Collection " << collName
                    << " already read, not reading it again" << endmsg;
            return StatusCode::SUCCESS;
        }

        const T *collection(nullptr);
        collection = static_cast<const T*>(m_eventframe.get(collName));
        if (collection == nullptr)
        {
            error() << "Collection " << collName << " does not exist."
                    << endmsg;
        }
        auto wrapper = new DataWrapper<T>;
        wrapper->setData(collection);
        m_podio_datawrappers.push_back(wrapper);
        return DataSvc::registerObject("/Event", "/" + collName, wrapper);
    }

    const podio::Frame& getEventFrame() const
    {
        return m_eventframe;
    }

    /// Resets caches of reader and event store, increases event counter
    void endOfRead();

    /// TODO: Make this private again after conversions have been properly solved
    podio::Frame& getMetaDataFrame()
    {
        return m_metadataframe;
    }

private:
    /// PODIO reader for ROOT files
    podio::ROOTFrameReader m_reader;
    /// PODIO Frame, used to initialise collections
    podio::Frame m_eventframe;
    /// PODIO Frame, used to store metadata
    podio::Frame m_metadataframe;
    /// Counter of the event number
    int m_eventNum { 0 };
    /// Number of events in the file / to process
    int m_numAvailableEvents { -1 };
    int m_requestedEventMax { -1 };
    /// Whether reading from file at all
    bool m_reading_from_file { false };

    SmartIF<IConversionSvc> m_cnvSvc;

    // Registry of data wrappers; needed for memory management
    std::vector<DataWrapperBase*> m_podio_datawrappers;

protected:
    /// ROOT file name the input is read from. Set by option filename
    std::vector<std::string> m_filenames;
    std::string m_filename;
    /// Jump to nth events at the beginning. Set by option FirstEventEntry
    /// This option is helpful when we want to debug an event in the middle of a file
    unsigned m_1stEvtEntry { 0 };
    bool m_bounds_check_needed { true };
};
#endif  // OVERLAYDATASVC_H
