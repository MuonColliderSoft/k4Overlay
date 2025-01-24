#ifndef OVERLAYDATASVC_H
#define OVERLAYDATASVC_H

#include "GaudiKernel/DataSvc.h"
#include "GaudiKernel/IConversionSvc.h"
#include "podio/CollectionBase.h"
#include "podio/CollectionIDTable.h"
#include "podio/Frame.h"
#include "podio/ROOTFrameReader.h"
#include "k4FWCore/DataWrapper.h"
#include "BackgroundReaderSvc.h"

#include "edm4hep/SimTrackerHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4hep/MCParticleCollection.h"
#include "edm4hep/CaloHitContributionCollection.h"

#include <set>
#include <utility>
#include <unordered_map>

using MCCollMap = std::unordered_map<std::string, edm4hep::MCParticleCollection>;
using CCCollMap = std::unordered_map<std::string, edm4hep::CaloHitContributionCollection>;
using SCCollMap = std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection>;
using STCollMap = std::unordered_map<std::string, edm4hep::SimTrackerHitCollection>;

// key is <collection type priority, collection name>
using CollNameKey = std::pair<int, std::string>;
using CollNameSet = std::set<CollNameKey>;

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
    StatusCode mergeFrame(const podio::Frame& frame);

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

    Gaudi::Property<std::vector<std::string>> m_filenames {
        this, "signal_files", {}, "Names of the signal files to read"
    };
    Gaudi::Property<unsigned> m_1stEvtEntry {
        this, "FirstEventEntry", 0, "First event to read"
    };
    Gaudi::Property<int> num_bib {
        this, "num_of_bib", 100, "Number of bib events to merge"
    };
    Gaudi::Property<vector<string>> coll_defs {
        this, "collections", {}, "Name and type of the collections to scan"
    };

    bool m_bounds_check_needed { true };

    ServiceHandle<IBackgroundReaderSvc> m_BIB1Svc { this, "BIBMuPlusSourceSvc", "BIBMuPlusSourceSvc" };
    ServiceHandle<IBackgroundReaderSvc> m_BIB2Svc { this, "BIBMuMinusSourceSvc", "BIBMuMinusSourceSvc" };
    ServiceHandle<IBackgroundReaderSvc> m_IPairSvc { this, "IPairSourceSvc", "IPairSourceSvc" };

    CollNameSet collname_set;
    MCCollMap mc_coll_map;
    CCCollMap cc_coll_map;
    SCCollMap sc_coll_map;
    STCollMap st_coll_map;
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

DECLARE_COMPONENT_WITH_ID(BackgroundReaderSvc, "BIBMuPlusSourceSvc")
DECLARE_COMPONENT_WITH_ID(BackgroundReaderSvc, "BIBMuMinusSourceSvc")
DECLARE_COMPONENT_WITH_ID(BackgroundReaderSvc, "IPairSourceSvc")
DECLARE_COMPONENT_WITH_ID(OverlayDataSvc, "OverlayDataSvc")

#endif  // OVERLAYDATASVC_H
