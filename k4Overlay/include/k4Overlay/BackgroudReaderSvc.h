#ifndef BackgroundReaderSvc_h
#define BackgroundReaderSvc_h

#include "GaudiKernel/IInterface.h"
#include "GaudiKernel/Service.h"
#include "GaudiKernel/extends.h"
#include "podio/Frame.h"
#include "podio/ROOTFrameReader.h"

#include <optional>

using std::string;
using std::vector;
using OptFrame = std::optional<podio::Frame>;

struct IBackgroundReaderSvc : virtual IInterface
{
    DeclareInterfaceID(IBackgroundReaderSvc, 1, 0);
    virtual OptFrame getEventFrame() = 0;
    virtual unsigned size() = 0;
};


class GAUDI_API BackgroundReaderSvc : public extends<Service, IBackgroundReaderSvc>
{
public:
    using extends::extends;  // inherit constructors
    BackgroundReaderSvc(const BackgroundReaderSvc&) = delete;
    BackgroundReaderSvc& operator=(const BackgroundReaderSvc&) = delete;

    StatusCode initialize() override final;
    OptFrame getEventFrame() override;
    unsigned size() override;
    StatusCode finalize() final;
private:
    Gaudi::Property<vector<string>> m_filenames { this, "inputs", {}, "Names of the files to read" };

    podio::ROOTFrameReader m_reader;
    unsigned curr_evn;
    unsigned total_evns;
};

#endif //BackgroundReaderSvc_h
