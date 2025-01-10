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
	BGFrameList evnFrames {};
	for (int k = 0; k < num_bib; k++)
	{
		auto bib1 = m_BIB1Svc->getEventFrame();
		if (!bib1)
		{
			always() << "Wrong frame for BIB1" << endmsg;
			return StatusCode::FAILURE;
		}
		evnFrames.emplace_back(std::move(bib1.value()));

		auto bib2 = m_BIB2Svc->getEventFrame();
		if (!bib2)
		{
			always() << "Wrong frame for BIB2" << endmsg;
			return StatusCode::FAILURE;
		}
		evnFrames.emplace_back(std::move(bib2.value()));

		auto ipp = m_IPairSvc->getEventFrame();
		if (!ipp)
		{
			always() << "Wrong frame for IPP" << endmsg;
			return StatusCode::FAILURE;
		}
		evnFrames.emplace_back(std::move(ipp.value()));
	}

	for (auto [coll_name, coll_type] : type_table)
    {
        if (handler_table[coll_name]->mergeEvents(evnFrames) == StatusCode::FAILURE)
        {
            always() << "Execution failure for " << coll_name << endmsg;
            return StatusCode::FAILURE;
        }
    }

    return StatusCode::SUCCESS;
}
