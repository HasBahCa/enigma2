#ifndef _lib_dvb_fastscan_h
#define _lib_dvb_fastscan_h

#include <lib/base/object.h>
#include <lib/dvb/idvb.h>

#ifndef SWIG

#include <lib/dvb/idemux.h>
#include <lib/dvb/esection.h>
#include <lib/dvb/logicalchanneldescriptor.h>

#include <dvbsi++/long_crc_section.h>
#include <dvbsi++/service_descriptor.h>
#include <dvbsi++/network_name_descriptor.h>
#include <dvbsi++/service_list_descriptor.h>
#include <dvbsi++/satellite_delivery_system_descriptor.h>

class FastScanService : public ServiceDescriptor
{
protected:
	unsigned originalNetworkId : 16;
	unsigned transportStreamId : 16;
	unsigned serviceId : 16;
	unsigned defaultVideoPid : 16;
	unsigned defaultAudioPid : 16;
	unsigned defaultVideoEcmPid : 16;
	unsigned defaultAudioEcmPid : 16;
	unsigned defaultPcrPid : 16;
	unsigned descriptorLoopLength : 16;

public:
	FastScanService(const uint8_t *const buffer);
	~FastScanService(void);

	uint16_t getOriginalNetworkId(void) const;
	uint16_t getTransportStreamId(void) const;
	uint16_t getServiceId(void) const;
	uint16_t getDefaultVideoPid(void) const;
	uint16_t getDefaultAudioPid(void) const;
	uint16_t getDefaultVideoEcmPid(void) const;
	uint16_t getDefaultAudioEcmPid(void) const;
	uint16_t getDefaultPcrPid(void) const;
};

typedef std::list<FastScanService*> FastScanServiceList;
typedef FastScanServiceList::iterator FastScanServiceListIterator;
typedef FastScanServiceList::const_iterator FastScanServiceListConstIterator;

class FastScanServicesSection : public LongCrcSection
{
protected:
	FastScanServiceList services;

public:
	FastScanServicesSection(const uint8_t * const buffer);
	~FastScanServicesSection(void);

	static const uint16_t LENGTH = 4096;
	static const int TID = 0xBD;
	static const uint32_t TIMEOUT = 5000;

	const FastScanServiceList *getServices(void) const;
};

class FastScanTransportStream
{
protected:
	unsigned transportStreamId : 16;
	unsigned originalNetworkId : 16;
	unsigned descriptorLoopLength : 16;

	SatelliteDeliverySystemDescriptor *deliverySystem;
	ServiceListDescriptor *serviceList;
	LogicalChannelDescriptor *logicalChannels;

public:
	FastScanTransportStream(const uint8_t *const buffer);
	~FastScanTransportStream(void);

	uint16_t getOriginalNetworkId(void) const;
	uint16_t getTransportStreamId(void) const;
	uint16_t getOrbitalPosition(void) const;
	uint32_t getFrequency(void) const;
	uint8_t getPolarization(void) const;
	uint8_t getRollOff(void) const;
	uint8_t getModulationSystem(void) const;
	uint8_t getModulation(void) const;
	uint32_t getSymbolRate(void) const;
	uint8_t getFecInner(void) const;

	const ServiceListItemList *getServiceList(void) const;
	const LogicalChannelList *getLogicalChannelList(void) const;
};

typedef std::list<FastScanTransportStream *> FastScanTransportStreamList;
typedef FastScanTransportStreamList::iterator FastScanTransportStreamListIterator;
typedef FastScanTransportStreamList::const_iterator FastScanTransportStreamListConstIterator;

class FastScanNetworkSection : public LongCrcSection, public NetworkNameDescriptor
{
protected:

	FastScanTransportStreamList transportStreams;

public:
	FastScanNetworkSection(const uint8_t * const buffer);
	~FastScanNetworkSection(void);

	const FastScanTransportStreamList *getTransportStreams(void) const;

	static const uint16_t LENGTH = 4096;
	static const int TID = 0xBC;
	static const uint32_t TIMEOUT = 5000;
};

class eDVBFastScanServicesSpec
{
	eDVBTableSpec m_spec;
public:
	eDVBFastScanServicesSpec(int pid)
	{
		m_spec.pid     = pid;
		m_spec.tid     = FastScanServicesSection::TID;
		m_spec.timeout = FastScanServicesSection::TIMEOUT;
		m_spec.flags   = eDVBTableSpec::tfAnyVersion |
			eDVBTableSpec::tfHaveTID | eDVBTableSpec::tfCheckCRC | eDVBTableSpec::tfHaveTimeout;
	}
	operator eDVBTableSpec &()
	{
		return m_spec;
	}
};

class eDVBFastScanNetworkSpec
{
	eDVBTableSpec m_spec;
public:
	eDVBFastScanNetworkSpec(int pid)
	{
		m_spec.pid     = pid;
		m_spec.tid     = FastScanNetworkSection::TID;
		m_spec.timeout = FastScanNetworkSection::TIMEOUT;
		m_spec.flags   = eDVBTableSpec::tfAnyVersion |
			eDVBTableSpec::tfHaveTID | eDVBTableSpec::tfCheckCRC | eDVBTableSpec::tfHaveTimeout;
	}
	operator eDVBTableSpec &()
	{
		return m_spec;
	}
};

template <class Section>
class eFastScanTable : public eTable<Section>
{
	std::set<int> seen;
public:
	int createTable(unsigned int nr, const __u8 *data, unsigned int max)
	{
		seen.insert(nr);
		tableProgress(seen.size(), max);
		return eTable<Section>::createTable(nr, data, max);
	}
	Signal2<void, int, int> tableProgress;
};

template <class Section>
class eFastScanFileTable : public eFastScanTable<Section>
{
public:
	void readFile(FILE *file)
	{
		if (!file) return;
		unsigned char buffer[4096];
		while (1)
		{
			if (fread(buffer, 3, 1, file) < 1) break;
			unsigned int sectionsize = ((buffer[1]) & 0xf) * 256 + buffer[2];
			if (fread(&buffer[3], sectionsize, 1, file) < 1) break;
			if (eFastScanTable<Section>::createTable(buffer[6], buffer, buffer[7] + 1) > 0) break;
		}
	}
};

#endif /* no SWIG */

class eFastScan: public Object, public iObject
{
	DECLARE_REF(eFastScan);

#ifndef SWIG
	eUsePtr<iDVBChannel> m_channel;
	ePtr<iDVBDemux> m_demux;
	bool originalNumbering;
	bool useFixedServiceInfo;
	std::string providerName, bouquetFilename;
	int m_pid;
	ePtr<eFastScanTable<FastScanServicesSection> > m_ServicesTable;
	ePtr<eFastScanTable<FastScanNetworkSection> > m_NetworkTable;

	void servicesTableProgress(int size, int max);
	void networkTableProgress(int size, int max);
	void servicesTableReady(int error);
	void networkTableReady(int error);

	void fillBouquet(eBouquet *bouquet, std::map<int, eServiceReferenceDVB> &numbered_channels);
	void parseResult();
#endif /* no SWIG */

public:
	eFastScan(int pid, const char *providername, bool originalnumbering = false, bool fixedserviceinfo = false);
	~eFastScan();

	void start(int frontendid = 0);
	void startFile(const char *fnt, const char *fst);

	PSignal1<void, int> scanProgress;
	PSignal1<void, int> scanCompleted;
};

#endif