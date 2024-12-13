gaudi_add_module(k4Overlay
                 SOURCES src/BackgroundReaderSvc.cpp
                 LINK GaudiKernel
                      GaudiAlg
                      podio
                      edm4hep
                      k4FWCore
)

target_include_directories(k4Overlay PUBLIC
    ${PROJECT_SOURCE_DIR}/k4Overlay/include/k4Overlay
)

