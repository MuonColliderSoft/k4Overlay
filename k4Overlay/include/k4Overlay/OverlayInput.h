#ifndef OVERLAYINPUT_H
#define OVERLAYINPUT_H

#include "Gaudi/Property.h"
#include "GaudiAlg/Consumer.h"

#include <string>
#include <vector>

class OverlayDataSvc;

using BaseClass_t = Gaudi::Functional::Traits::BaseClass_t<Gaudi::Algorithm>;

class OverlayInput final : public Gaudi::Functional::Consumer<void(), BaseClass_t>
{
public:
    OverlayInput(const std::string &name, ISvcLocator *svcLoc);
    void operator()() const override;

    StatusCode initialize() final;

private:
    template<typename T> void maybeRead(std::string_view collName) const;
    void fillReaders();
    // Name of collections to read. Set by option collections (this is temporary)
    Gaudi::Property<std::vector<std::string>> m_collectionNames {
        this, "collections",  { }, "Collections that should be read (default all)"
    };
    // Data service: needed to register objects and get collection IDs. Just an observing pointer.
    OverlayDataSvc *m_overlayDataSvc;
    mutable std::map<std::string_view, std::function<void(std::string_view)>> m_readers;
};

#endif //OVERLAYINPUT_H
