#ifndef _ots_CFOFrontEndInterface_h_
#define _ots_CFOFrontEndInterface_h_

#include <map>
#include <string>
#include "otsdaq-mu2e/CFOandDTCCore/CFOandDTCCoreVInterface.h"

#include "cfoInterfaceLib/CFO_Registers.h"

namespace ots
{
class CFOFrontEndInterface : public CFOandDTCCoreVInterface
{
	// clang-format off

  public:
	CFOFrontEndInterface(const std::string&       interfaceUID,
	                     const ConfigurationTree& theXDAQContextConfigTree,
	                     const std::string&       interfaceConfigurationPath);

	virtual ~CFOFrontEndInterface(void);

	// state machine
	//----------------
	void 								configure					(void) override;
	void 								halt						(void) override;
	void 								pause						(void) override;
	void 								resume						(void) override;
	void 								start						(std::string runNumber) override;
	void 								stop						(void) override;
	bool 								running						(void) override;

	// CFO specific items
	//----------------
	float 								MeasureLoopback				(int linkToLoopback);
	virtual std::string					readStatus					(void) override;
	int  								getLinkStatus				(void);

	// hardware access
	//----------------
//	void universalRead(char* address, char* readValue) override;
//	void universalWrite(char* address, char* writeValue) override;
//	dtc_data_t							registerRead				(dtc_address_t address);
	virtual	dtc_data_t					registerWrite				(dtc_address_t address, dtc_data_t dataToWrite) override;  // return read value after having written dataToWrite


	float 								delay[8][6][8];
	float 								delay_rms[8][6][8];
	float 								delay_failed[8][6][8];

  protected: 
  	

  private:
	void 								registerFEMacros			(void);
	
//	char                   devfile_[11];
//	int                    fd_;
//	int                    dtc_ = -1;
	CFOLib::CFO_Registers* 				thisCFO_;
	//int                    configure_clock_ = 0;

	//m_ioc_reg_access_t 					reg_access_;

	int          						loopback_distribution_[10000];
	unsigned int 						min_distribution_;
	unsigned int 						max_distribution_;
	float        						average_loopback_;
	float        						rms_loopback_;
	float        						failed_loopback_;
	unsigned int 						initial_9100_;
	unsigned int 						initial_9114_;
	unsigned int 						initial_91a0_;
	unsigned int 						initial_9154_;

  public:
	void 								WriteCFO						(__ARGS__);
	void 								ReadCFO							(__ARGS__);
	void 								ResetRunplan					(__ARGS__);
};

// clang-format on
}  // namespace ots

#endif
