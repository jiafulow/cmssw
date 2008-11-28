import FWCore.ParameterSet.Config as cms

process = cms.Process("FedCablingReader")

process.MessageLogger = cms.Service("MessageLogger",
    debugModules = cms.untracked.vstring('*'),
    cablingReader = cms.untracked.PSet(
        threshold = cms.untracked.string('DEBUG')
    ),
    destinations = cms.untracked.vstring('cablingReader.log')
)

process.source = cms.Source("EmptySource",
    numberEventsInRun = cms.untracked.uint32(1),
    firstRun = cms.untracked.uint32(1)
)

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

process.load("CalibTracker.SiStripESProducers.SiStripFedCablingFakeESSource_cfi")

process.poolDBESSource = cms.ESSource("PoolDBESSource",
   BlobStreamerName = cms.untracked.string('TBufferBlobStreamingService'),
   DBParameters = cms.PSet(
        messageLevel = cms.untracked.int32(2),
        authenticationPath = cms.untracked.string('/afs/cern.ch/cms/DB/conddb')
    ),
    timetype = cms.untracked.string('runnumber'),
    connect = cms.string('sqlite_file:dummy2.db'),
    toGet = cms.VPSet(cms.PSet(
        record = cms.string('SiStripFedCablingRcd'),
        tag = cms.string('SiStripFedCabling_30X')
    ))
)

process.load("Configuration.StandardSequences.Geometry_cff")
process.TrackerDigiGeometryESModule.applyAlignment = False
process.SiStripConnectivity = cms.ESProducer("SiStripConnectivity")
process.SiStripRegionConnectivity = cms.ESProducer("SiStripRegionConnectivity",
                                                   EtaDivisions = cms.untracked.uint32(20),
                                                   PhiDivisions = cms.untracked.uint32(20),
                                                   EtaMax = cms.untracked.double(2.5)
)

process.fedcablingreader = cms.EDFilter("SiStripFedCablingReader",
                                        PrintFecCabling = cms.untracked.bool(True),
                                        PrintDetCabling = cms.untracked.bool(True),
                                        PrintRegionCabling = cms.untracked.bool(True)
)

process.p1 = cms.Path(process.fedcablingreader)


