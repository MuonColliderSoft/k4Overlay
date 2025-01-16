#ifndef OVERLAYDATASVC_H
#define OVERLAYDATASVC_H

#include "GaudiKernel/DataSvc.h"
#include "GaudiKernel/IConversionSvc.h"
#include "podio/CollectionBase.h"
#include "podio/CollectionIDTable.h"
#include "podio/Frame.h"
#include "podio/ROOTFrameReader.h"
#include "k4FWCore/DataWrapper.h"

class DataWrapperBase;

template<typename T> class MetaDataHandle;

class OverlayDataSvc: public DataSvc
{
    template<typename T> friend class MetaDataHandle;

public:

    OverlayDataSvc(const std::string &name, ISvcLocator *svc) : DataSvc(name, svc) {}
    virtual ~OverlayDataSvc() {}

    StatusCode initialize() final;
    StatusCode reinitialize() final;
    StatusCode finalize() final;
    StatusCode clearStore() final;
    StatusCode i_setRoot(std::string root_path, IOpaqueAddress *pRootAddr) final;
    StatusCode i_setRoot(std::string root_path, DataObject *pRootObj) final;

    // Use DataSvc functionality except where we override
    using DataSvc::registerObject;
    /// Overriding standard behaviour of evt service
    /// Register object with the data store.
    virtual StatusCode registerObject(std::string_view parentPath,
            std::string_view fullPath, DataObject *pObject) override final;

    const std::string_view getCollectionType(const std::string &collName);

    template<typename T> StatusCode readCollection(const std::string &collName);

    const podio::Frame& getEventFrame() const { return m_eventframe; }

    /// Resets caches of reader and event store, increases event counter
    void endOfRead();

    /// TODO: Make this private again after conversions have been properly solved
    podio::Frame& getMetaDataFrame() { return m_metadataframe; }

private:
    StatusCode readFrames();

    podio::ROOTFrameReader m_reader;
    podio::Frame m_eventframe;
    podio::Frame m_metadataframe;
    int m_eventNum { 0 };
    int m_numAvailableEvents { -1 };
    int m_requestedEventMax { -1 };
    bool m_reading_from_file { false };

    SmartIF<IConversionSvc> m_cnvSvc;

    // Registry of data wrappers; needed for memory management
    std::vector<DataWrapperBase*> m_podio_datawrappers;

    Gaudi::Property<std::vector<std::string>> m_filenames { this, "inputs", {}, "Names of the files to read" };
    Gaudi::Property<unsigned> m_1stEvtEntry { this, "FirstEventEntry", 0, "First event to read" };
    bool m_bounds_check_needed { true };
};

template<typename T>
StatusCode OverlayDataSvc::readCollection(const std::string &collName)
{
    DataObject *objectPtr = nullptr;
    if (DataSvc::findObject("/Event", "/" + collName, objectPtr))
    {
        debug() << "Collection " << collName << " already read, not reading it again" << endmsg;
        return StatusCode::SUCCESS;
    }

    const T *collection(nullptr);
    collection = static_cast<const T*>(m_eventframe.get(collName));
    if (collection == nullptr)
    {
        error() << "Collection " << collName << " does not exist." << endmsg;
    }
    auto wrapper = new DataWrapper<T>;
    wrapper->setData(collection);
    m_podio_datawrappers.push_back(wrapper);
    return DataSvc::registerObject("/Event", "/" + collName, wrapper);
}

#endif  // OVERLAYDATASVC_H
