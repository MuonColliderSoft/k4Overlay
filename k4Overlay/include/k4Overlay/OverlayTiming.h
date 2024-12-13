#ifndef OverlayTiming_h
#define OverlayTiming_h

#include "GaudiAlg/GaudiAlgorithm.h"
#include "edm4hep/EventHeader.h"
#include "edm4hep/SimTrackerHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4hep/MCParticleCollection.h"
#include "edm4hep/CaloHitContribution.h"
#include "k4FWCore/DataWrapper.h"
#include "BackgroundReaderSvc.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

DECLARE_COMPONENT_WITH_ID(BackgroundReaderSvc, "BIBMuPlusSourceSvc")
DECLARE_COMPONENT_WITH_ID(BackgroundReaderSvc, "BIBMuMinusSourceSvc")
DECLARE_COMPONENT_WITH_ID(BackgroundReaderSvc, "IPairSourceSvc")

using std::string;
using std::vector;

class EDMAbstractHandler
{
public:
    virtual StatusCode cloneSignal() = 0;
    virtual StatusCode mergeEvent(const OptFrame& evnFrame) = 0;
    virtual StatusCode flush() = 0;
};

template <class EDMEntity>
class EDMEntityHandler : public EDMAbstractHandler
{
public:

    using EDMEntityWrapper = DataWrapper<EDMEntity>;
    using EDMEntityHandle = DataObjectHandle<EDMEntityWrapper>;

    EDMEntityHandler(GaudiAlgorithm* algo, string coll_name) :
        m_algo(algo),
        c_name(coll_name)
    {
        in_handle = new EDMEntityHandle("/Event/" + coll_name, Gaudi::DataHandle::Reader, algo);
        algo->declareProperty("Collection_" + coll_name, *(in_handle));

        out_handle = new EDMEntityHandle("/Event/Overlay" + coll_name, Gaudi::DataHandle::Writer, algo);
        algo->declareProperty("Overlay_" + coll_name, *(out_handle));
    }

    virtual ~EDMEntityHandler()
    {
        delete in_handle;
        delete out_handle;
    }

    StatusCode cloneSignal() override
    {
        if (!in_handle->isValid()) return StatusCode::FAILURE;
        EDMEntityWrapper* in_wrapper = in_handle->get();
        const EDMEntity* input_coll = in_wrapper->getData();

        coll_buff.clear();
        for (auto item : *input_coll)
        {
            coll_buff.push_back(item.clone());
        }
        return StatusCode::SUCCESS;
    }

    StatusCode mergeEvent(const OptFrame& evnFrame) override
    {
        if (!evnFrame) return StatusCode::FAILURE;

        const EDMEntity* b_coll = static_cast<const EDMEntity*>(evnFrame.value().get(c_name));
        for (auto item : *b_coll)
        {
            coll_buff.push_back(item.clone());
        }
        return StatusCode::SUCCESS;
    }

    StatusCode flush() override
    {
        EDMEntity output_coll;
        for (auto item : coll_buff) output_coll.push_back(item);

        EDMEntityWrapper* out_wrapper = new EDMEntityWrapper(std::move(output_coll));
        out_handle->put(std::unique_ptr<EDMEntityWrapper>(out_wrapper));

        return StatusCode::SUCCESS;
    }

private:
    GaudiAlgorithm* m_algo;
    string c_name;
    EDMEntityHandle* in_handle;
    EDMEntityHandle* out_handle;

    vector<typename EDMEntity::value_type> coll_buff;
};

class OverlayTiming : public GaudiAlgorithm
{
public:
    OverlayTiming(const std::string& name, ISvcLocator* pSvcLocator);
    OverlayTiming(const OverlayTiming&) = delete;
    virtual ~OverlayTiming() override {}

    StatusCode initialize() override;
    StatusCode finalize() override;
    StatusCode execute() override;

    OverlayTiming& operator=(const OverlayTiming&) = delete;
private:
    using STHandler = EDMEntityHandler<edm4hep::SimTrackerHitCollection>;
    using SCHandler = EDMEntityHandler<edm4hep::SimCalorimeterHitCollection>;
    using MCHandler = EDMEntityHandler<edm4hep::MCParticleCollection>;

    ServiceHandle<IBackgroundReaderSvc> m_BIB1Svc{ this, "BIBMuPlusSourceSvc", "BIBMuPlusSourceSvc" };
    ServiceHandle<IBackgroundReaderSvc> m_BIB2Svc{ this, "BIBMuMinusSourceSvc", "BIBMuMinusSourceSvc" };
    ServiceHandle<IBackgroundReaderSvc> m_IPairSvc{ this, "IPairSourceSvc", "IPairSourceSvc" };
    Gaudi::Property<vector<string>> coll_defs { this, "collections", {}, "Name and type of the collections to scan" };
    Gaudi::Property<int> num_bib { this, "num_of_bib", 100, "Number of bib events to merge" };

    std::unordered_map<string, string> type_table;
    std::unordered_map<string, EDMAbstractHandler*> handler_table;
};

#endif //OverlayTiming_h
