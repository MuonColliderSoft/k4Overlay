#include "OverlayTiming.h"


DECLARE_COMPONENT(OverlayTiming)

OverlayTiming::OverlayTiming(const std::string& name, ISvcLocator* pSvcLocator) :
    GaudiAlgorithm::GaudiAlgorithm(name, pSvcLocator)
{}

StatusCode OverlayTiming::initialize()
{
    auto sc = GaudiAlgorithm::initialize();
    if ( !sc ) return sc;

    for (string item : coll_defs)
    {
        auto idx = item.find("=");
        if (idx == item.npos) continue;
        string coll_name = item.substr(0, idx);
        string coll_type = item.substr(idx + 1, item.size());

        type_table.emplace(coll_name, coll_type);


        if (coll_type == "edm4hep::SimTrackerHit")
        {
            handler_table.emplace(coll_name, new STHandler(this, coll_name));
        }
        else if (coll_type == "edm4hep::SimCalorimeterHit")
        {
            handler_table.emplace(coll_name, new SCHandler(this, coll_name));
        }
        else if (coll_type == "edm4hep::MCParticle")
        {
            handler_table.emplace(coll_name, new MCHandler(this, coll_name));
        }
    }

    return StatusCode::SUCCESS;
}

StatusCode OverlayTiming::finalize()
{
    for (auto [coll_name, c_handler] : handler_table) delete c_handler;
    handler_table.clear();

    return GaudiAlgorithm::finalize();
}

StatusCode OverlayTiming::execute()
{
    for (auto [coll_name, coll_type] : type_table)
    {
        if (handler_table[coll_name]->cloneSignal() == StatusCode::FAILURE)
        {
            always() << "Execution failure for " << coll_name << endmsg;
            return StatusCode::FAILURE;
        }

        for (int k = 0; k < num_bib; k++)
        {
            auto f_plus = m_BIB1Svc->getEventFrame();
            auto f_minus = m_BIB2Svc->getEventFrame();
            auto f_ipp = m_IPairSvc->getEventFrame();

            for (auto [coll_name, coll_type] : type_table)
            {
                if (handler_table[coll_name]->mergeEvent(f_plus) == StatusCode::FAILURE
                        || handler_table[coll_name]->mergeEvent(f_minus) == StatusCode::FAILURE
                        || handler_table[coll_name]->mergeEvent(f_ipp) == StatusCode::FAILURE)
                {
                    always() << "Merging failure for " << coll_name << endmsg;
                    return StatusCode::FAILURE;
                }
            }
        }

        if (handler_table[coll_name]->flush() == StatusCode::FAILURE)
        {
            always() << "Flush failure for " << coll_name << endmsg;
            return StatusCode::FAILURE;
        }
   }

    return StatusCode::SUCCESS;
}
