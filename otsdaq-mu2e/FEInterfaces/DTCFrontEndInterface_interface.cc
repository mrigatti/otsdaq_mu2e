#include "otsdaq-mu2e/FEInterfaces/DTCFrontEndInterface.h"
#include "otsdaq/Macros/BinaryStringMacros.h"
#include "otsdaq/Macros/InterfacePluginMacros.h"
#include "otsdaq/FECore/MakeInterface.h"

#include <fstream>


using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "DTCFrontEndInterface"


// // some global variables, probably a bad idea. But temporary
// std::string RunDataFN = "";
// std::fstream runDataFile_;
// int FEWriteFile = 0;
// bool artdaqMode_ = true;

//=========================================================================================
DTCFrontEndInterface::DTCFrontEndInterface(
    const std::string&       interfaceUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       interfaceConfigurationPath)
    : CFOandDTCCoreVInterface(
          interfaceUID, theXDAQContextConfigTree, interfaceConfigurationPath)
    , thisDTC_(0)
    , EmulatedCFO_(0)
{
	__FE_COUT__ << "instantiate DTC... " << interfaceUID << " "
	            << theXDAQContextConfigTree << " " << interfaceConfigurationPath << __E__;

	emulate_cfo_ = getSelfNode().getNode("EmulateCFO").getValue<bool>();

	if(emulatorMode_)
	{
		__FE_COUT__ << "Emulator DTC mode starting up..." << __E__;
		createROCs();
		registerFEMacros();
		return;
	}
	// else not emulator mode

	

	{
		uint32_t lossOfLockReadData = registerRead(0x93c8); //read loss-of-lock counter
		__FE_COUTV__(lossOfLockReadData);
	}


	unsigned dtc_class_roc_mask = 0;
	// create roc mask for DTC
	{
		std::vector<std::pair<std::string, ConfigurationTree>> rocChildren =
		    Configurable::getSelfNode().getNode("LinkToROCGroupTable").getChildren();

		roc_mask_ = 0;

		for(auto& roc : rocChildren)
		{
			__FE_COUT__ << "roc uid " << roc.first << __E__;
			bool enabled = roc.second.getNode("Status").getValue<bool>();
			__FE_COUT__ << "roc enable " << enabled << __E__;

			if(enabled)
			{
				int linkID = roc.second.getNode("linkID").getValue<int>();
				roc_mask_ |= (0x1 << linkID);
				dtc_class_roc_mask |=
				    (0x1 << (linkID * 4));  // the DTC class instantiation expects each
				                            // ROC has its own hex nibble
			}
		}

		__FE_COUT__ << "DTC roc_mask_ = 0x" << std::hex << roc_mask_ << std::dec << __E__;
		__FE_COUT__ << "roc_mask to instantiate DTC class = 0x" << std::hex
		            << dtc_class_roc_mask << std::dec << __E__;

	}  // end create roc mask

	// instantiate DTC with the appropriate ROCs enabled
	std::string expectedDesignVersion = "";
	DTCLib::DTC_SimMode        mode   = emulate_cfo_?DTCLib::DTC_SimMode_NoCFO:DTCLib::DTC_SimMode_Disabled;
	bool 		skipInit			  = getConfigurationManager()
	        ->getNode("/Mu2eGlobalsTable/SyncDemoConfig/SkipCFOandDTCConfigureSteps")
	        .getValue<bool>();

	__COUT__ << "DTC arguments..." << std::endl;
	__COUTV__(mode);
	__COUTV__(device_);
	__COUTV__(dtc_class_roc_mask);
	__COUTV__(expectedDesignVersion);
	__COUTV__(skipInit);
	__COUT__ << "END END DTC arguments..." << std::endl;

	{
		uint32_t lossOfLockReadData = registerRead(0x93c8); //read loss-of-lock counter
		__FE_COUTV__(lossOfLockReadData);
	}

	thisDTC_ = new DTCLib::DTC(mode, device_, dtc_class_roc_mask, expectedDesignVersion, skipInit);

	{
		uint32_t lossOfLockReadData = registerRead(0x93c8); //read loss-of-lock counter
		__FE_COUTV__(lossOfLockReadData);
	}


	createROCs();
	registerFEMacros();

	{
		uint32_t lossOfLockReadData = registerRead(0x93c8); //read loss-of-lock counter
		__FE_COUTV__(lossOfLockReadData);
	}

	// DTC-specific info
	dtc_location_in_chain_ =
	    getSelfNode().getNode("LocationInChain").getValue<unsigned int>();

	// check if any ROCs should be DTC-hardware emulated ROCs
	if(skipInit)
	{
		__FE_COUT_INFO__ << "Skipping configure steps!" << __E__;
	}
	else
	{
		std::vector<std::pair<std::string, ConfigurationTree>> rocChildren =
		    Configurable::getSelfNode().getNode("LinkToROCGroupTable").getChildren();

		int dtcHwEmulateROCmask = 0;
		for(auto& roc : rocChildren)
		{
			bool enabled = roc.second.getNode("EmulateInDTCHardware").getValue<bool>();

			if(enabled)
			{
				int linkID = roc.second.getNode("linkID").getValue<int>();
				__FE_COUT__ << "roc uid '" << roc.first << "' at link=" << linkID
				            << " is DTC-hardware emulated!" << __E__;
				dtcHwEmulateROCmask |= (1 << linkID);
			}
		}

		__FE_COUT__ << "Writing DTC-hardware emulation mask: 0x" << std::hex
		            << dtcHwEmulateROCmask << std::dec << __E__;
		registerWrite(0x9110, dtcHwEmulateROCmask);
		__FE_COUT__ << "End check for DTC-hardware emulated ROCs." << __E__;
	}  // end check if any ROCs should be DTC-hardware emulated ROCs

	__FE_COUT_INFO__ << "DTC instantiated with name: "
			<< device_name_ << " dtc_location_in_chain_ = "
			<< dtc_location_in_chain_ << " talking to /dev/mu2e" << device_ << __E__;
}  // end constructor()

//==========================================================================================
DTCFrontEndInterface::~DTCFrontEndInterface(void)
{
	{
		uint32_t lossOfLockReadData = registerRead(0x93c8); //read loss-of-lock counter
		__FE_COUTV__(lossOfLockReadData);
	}

	// destroy ROCs before DTC destruction
	rocs_.clear();

	{
		uint32_t lossOfLockReadData = registerRead(0x93c8); //read loss-of-lock counter
		__FE_COUTV__(lossOfLockReadData);
	}

	if(thisDTC_)
		delete thisDTC_;

	__FE_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
void DTCFrontEndInterface::configureSlowControls(void)
{
	__FE_COUT__ << "Configuring slow controls..." << __E__;

	// parent configure adds DTC slow controls channels
	FEVInterface::configureSlowControls();  // also resets DTC-proper channels

	__FE_COUT__ << "DTC '" << getInterfaceUID()
	            << "' slow controls channel count (BEFORE considering ROCs): "
	            << mapOfSlowControlsChannels_.size() << __E__;

	mapOfROCSlowControlsChannels_.clear();  // reset ROC channels

	// now add ROC slow controls channels
	ConfigurationTree ROCLink =
	    Configurable::getSelfNode().getNode("LinkToROCGroupTable");
	if(!ROCLink.isDisconnected())
	{
		std::vector<std::pair<std::string, ConfigurationTree>> rocChildren =
		    ROCLink.getChildren();

		unsigned int initialChannelCount;

		for(auto& rocChildPair : rocChildren)
		{
			initialChannelCount = mapOfROCSlowControlsChannels_.size();

			FEVInterface::addSlowControlsChannels(
			    rocChildPair.second.getNode("LinkToSlowControlsChannelTable"),
			    "/" + rocChildPair.first /*subInterfaceID*/,
			    &mapOfROCSlowControlsChannels_);

			__FE_COUT__ << "ROC '" << getInterfaceUID() << "/" << rocChildPair.first
			            << "' slow controls channel count: "
			            << mapOfROCSlowControlsChannels_.size() - initialChannelCount
			            << __E__;

		}  // end ROC children loop

	}  // end ROC channel handling
	else
		__FE_COUT__ << "ROC link disconnected, assuming no ROCs" << __E__;

	__FE_COUT__ << "DTC '" << getInterfaceUID()
	            << "' slow controls channel count (AFTER considering ROCs): "
	            << mapOfSlowControlsChannels_.size() +
	                   mapOfROCSlowControlsChannels_.size()
	            << __E__;

	__FE_COUT__ << "Done configuring slow controls." << __E__;

}  // end configureSlowControls()

//==============================================================================
// virtual in case channels are handled in multiple maps, for example
void DTCFrontEndInterface::resetSlowControlsChannelIterator(void)
{
	// call parent
	FEVInterface::resetSlowControlsChannelIterator();

	currentChannelIsInROC_ = false;
}  // end resetSlowControlsChannelIterator()

//==============================================================================
// virtual in case channels are handled in multiple maps, for example
FESlowControlsChannel* DTCFrontEndInterface::getNextSlowControlsChannel(void)
{
	// if finished with DTC slow controls channels, move on to ROC list
	if(slowControlsChannelsIterator_ == mapOfSlowControlsChannels_.end())
	{
		slowControlsChannelsIterator_ = mapOfROCSlowControlsChannels_.begin();
		currentChannelIsInROC_        = true;  // switch to ROC mode
	}

	// if finished with ROC list, then done
	if(slowControlsChannelsIterator_ == mapOfROCSlowControlsChannels_.end())
		return nullptr;

	if(currentChannelIsInROC_)
	{
		std::vector<std::string> uidParts;
		StringMacros::getVectorFromString(
		    slowControlsChannelsIterator_->second.interfaceUID_,
		    uidParts,
		    {'/'} /*delimiters*/);
		if(uidParts.size() != 2)
		{
			__FE_SS__ << "Illegal ROC slow controls channel name '"
			          << slowControlsChannelsIterator_->second.interfaceUID_
			          << ".' Format should be DTC/ROC." << __E__;
		}
		currentChannelROCUID_ =
		    uidParts[1];  // format DTC/ROC names, take 2nd part as ROC UID
	}
	return &(
	    (slowControlsChannelsIterator_++)->second);  // return iterator, then increment
}  // end getNextSlowControlsChannel()

//==============================================================================
// virtual in case channels are handled in multiple maps, for example
unsigned int DTCFrontEndInterface::getSlowControlsChannelCount(void)
{
	return mapOfSlowControlsChannels_.size() + mapOfROCSlowControlsChannels_.size();
}  // end getSlowControlsChannelCount()

//==============================================================================
// virtual in case read should be different than universalread
void DTCFrontEndInterface::getSlowControlsValue(FESlowControlsChannel& channel,
                                                std::string&           readValue)
{
	__FE_COUTV__(currentChannelIsInROC_);
	__FE_COUTV__(currentChannelROCUID_);
	__FE_COUTV__(universalDataSize_);
	if(!currentChannelIsInROC_)
	{
		readValue.resize(universalDataSize_);
		universalRead(channel.getUniversalAddress(), &readValue[0]);
	}
	else
	{
		auto rocIt = rocs_.find(currentChannelROCUID_);
		if(rocIt == rocs_.end())
		{
			__FE_SS__ << "ROC UID '" << currentChannelROCUID_
			          << "' was not found in ROC map." << __E__;
			ss << "Here are the existing ROCs: ";
			bool first = true;
			for(auto& rocPair : rocs_)
				if(!first)
					ss << ", " << rocPair.first;
				else
				{
					ss << rocPair.first;
					first = false;
				}
			ss << __E__;
			__FE_SS_THROW__;
		}
		readValue.resize(universalDataSize_);
		*((uint16_t*)(&readValue[0])) =
		    rocIt->second->readRegister(*((uint16_t*)channel.getUniversalAddress()));
	}

	__FE_COUTV__(readValue.size());
}  // end getNextSlowControlsChannel()

//==============================================================================
void DTCFrontEndInterface::registerFEMacros(void)
{
	__FE_COUT__ << "Registering DTC FE Macros..." << __E__;	

	mapOfFEMacroFunctions_.clear();

	// clang-format off

	registerFEMacroFunction(
		"ROC_WriteBlock",  // feMacroName
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::WriteROCBlock),  // feMacroFunction
					std::vector<std::string>{"rocLinkIndex", "block", "address", "writeData"},
					std::vector<std::string>{},  // namesOfOutputArgs
					1);                          // requiredUserPermissions

	registerFEMacroFunction(
		"ROC_MultipleRead",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::ReadROCBlock),
				        std::vector<std::string>{"rocLinkIndex", "numberOfWords", "address", "incrementAddress"},
					std::vector<std::string>{"readData"},
					1);  // requiredUserPermissions
					
	registerFEMacroFunction(
		"ROC_ReadBlock",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::BlockReadROC),
				        std::vector<std::string>{"rocLinkIndex", "block", "address"},
					std::vector<std::string>{"readData"},
					1);  // requiredUserPermissions					

	registerFEMacroFunction(
		"ROC_Write",  // feMacroName
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::WriteROC),  // feMacroFunction
					std::vector<std::string>{"rocLinkIndex", "address", "writeData"},
					std::vector<std::string>{},  // namesOfOutput
					1);                          // requiredUserPermissions

	registerFEMacroFunction(
		"ROC_Read",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::ReadROC),                  // feMacroFunction
					std::vector<std::string>{"rocLinkIndex", "address"},  // namesOfInputArgs
					std::vector<std::string>{"readData"},
					1);  // requiredUserPermissions
					
	registerFEMacroFunction(
		"DTC_Write",  // feMacroName
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::WriteDTC),  // feMacroFunction
					std::vector<std::string>{"address", "writeData"},
					std::vector<std::string>{},  // namesOfOutput
					1);                          // requiredUserPermissions

	registerFEMacroFunction(
		"DTC_Read",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::ReadDTC),                  // feMacroFunction
					std::vector<std::string>{"address"},  // namesOfInputArgs
					std::vector<std::string>{"readData"},
					1);  // requiredUserPermissions

	registerFEMacroFunction(
		"DTC_Reset",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::DTCReset),
					std::vector<std::string>{},
					std::vector<std::string>{},
					1);  // requiredUserPermissions

	registerFEMacroFunction(
		"DTC_HighRate_DCS_Check",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::DTCHighRateDCSCheck),
					std::vector<std::string>{"rocLinkIndex","loops","baseAddress",
						"correctRegisterValue0","correctRegisterValue1"},
					std::vector<std::string>{},
					1);  // requiredUserPermissions
					
	registerFEMacroFunction(
		"DTC_HighRate_DCS_Block_Check",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::DTCHighRateBlockCheck),
					std::vector<std::string>{"rocLinkIndex","loops","baseAddress",
						"correctRegisterValue0","correctRegisterValue1"},
					std::vector<std::string>{},
					1);  // requiredUserPermissions

	registerFEMacroFunction(
		"DTC_SendHeartbeatAndDataRequest",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::DTCSendHeartbeatAndDataRequest),
					std::vector<std::string>{"numberOfRequests","timestampStart","useSWCFOEmulator","rocMask"},
					std::vector<std::string>{"readData"},
					1);  // requiredUserPermissions					
					
	registerFEMacroFunction(
		"Reset Loss-of-Lock Counter",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::ResetLossOfLockCounter),
					std::vector<std::string>{},
					std::vector<std::string>{						
						"Upstream Rx Lock Loss Count"},
					1);  // requiredUserPermissions
	registerFEMacroFunction(
		"Read Loss-of-Lock Counter",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::ReadLossOfLockCounter),
					std::vector<std::string>{},
					std::vector<std::string>{						
						"Upstream Rx Lock Loss Count"},
					1);  // requiredUserPermissions

	registerFEMacroFunction(
		"Get Upstream Rx Control Link Status",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::GetUpstreamControlLinkStatus),
					std::vector<std::string>{},
					std::vector<std::string>{						
						"Upstream Rx Lock Loss Count",
						"Upstream Rx CDR Lock Status",
						"Upstream Rx PLL Lock Status",
						"Jitter Attenuator Reset",
						"Jitter Attenuator Input Select",
						"Jitter Attenuator Loss-of-Signal",
						"Reset Done"},
					1 /* requiredUserPermissions */,
					"*" /* allowedCallingFEs */,
					"Get the status of the upstream control link, which is the forwarded synchronization and control sourced from the CFO through DTC daisy-chains." /* feMacroTooltip */
	);

	registerFEMacroFunction(
		"Select Jitter Attenuator Source",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&DTCFrontEndInterface::SelectJitterAttenuatorSource),
				        std::vector<std::string>{"Source (0 is Control Link Rx, 1 is RJ45, 2 is FPGA FMC)"},
						std::vector<std::string>{"Register Write Results"},
					1);  // requiredUserPermissions
	
					
	std::string value = FEVInterface::getFEMacroConstArgument(std::vector<std::pair<const std::string,std::string>>{
		
	std::make_pair("Link to Shutdown (0-7, 6 is Control)","0")},
	"Link to Shutdown (0-7, 6 is Control)");
			
			__COUTV__(value);


	{ //add ROC FE Macros
		__FE_COUT__ << "Getting children ROC FEMacros..." << __E__;
		rocFEMacroMap_.clear();
		for(auto& roc : rocs_)
		{
			auto feMacros = roc.second->getMapOfFEMacroFunctions();
			for(auto& feMacro:feMacros)
			{
				__FE_COUT__ << roc.first << "::" << feMacro.first << __E__;

				//make DTC FEMacro forwarding to ROC FEMacro				
                std::string macroName =
                                "Link" +
                                std::to_string(roc.second->getLinkID()) +
                                "_" + roc.first + "_" +
                                feMacro.first;
                __FE_COUTV__(macroName);
                std::vector<std::string> inputArgs,outputArgs;
                //inputParams.push_back("ROC_UID");
                //inputParams.push_back("ROC_FEMacroName");
				for(auto& inArg: feMacro.second.namesOfInputArguments_)
					inputArgs.push_back(inArg);
				for(auto& outArg: feMacro.second.namesOfOutputArguments_)
					outputArgs.push_back(outArg);

				__FE_COUTV__(StringMacros::vectorToString(inputArgs));
				__FE_COUTV__(StringMacros::vectorToString(outputArgs));

				rocFEMacroMap_.emplace(std::make_pair(macroName,
						std::make_pair(roc.first,feMacro.first)));

				registerFEMacroFunction(macroName,
						static_cast<FEVInterface::frontEndMacroFunction_t>(
								&DTCFrontEndInterface::RunROCFEMacro),
								inputArgs,
								outputArgs,
								1);  // requiredUserPermissions
			}
		}
	} //end add ROC FE Macros

	// clang-format on
	
	CFOandDTCCoreVInterface::registerCFOandDTCFEMacros();

}  // end registerFEMacros()

//==============================================================================
void DTCFrontEndInterface::createROCs(void)
{
	rocs_.clear();

	std::vector<std::pair<std::string, ConfigurationTree>> rocChildren =
	    Configurable::getSelfNode().getNode("LinkToROCGroupTable").getChildren();

	// instantiate vector of ROCs
	for(auto& roc : rocChildren)
		if(roc.second.getNode("Status").getValue<bool>())
		{
			__FE_COUT__
			    << "ROC Plugin Name: "
			    << roc.second.getNode("ROCInterfacePluginName").getValue<std::string>()
			    << std::endl;
			__FE_COUT__ << "ROC Name: " << roc.first << std::endl;

			try
			{
				__COUTV__(theXDAQContextConfigTree_.getValueAsString());
				__COUTV__(
				    roc.second.getNode("ROCInterfacePluginName").getValue<std::string>());

				// Note: FEVInterface makeInterface returns a unique_ptr
				//	and we want to verify that ROCCoreVInterface functionality
				//	is there, so we do an intermediate dynamic_cast to check
				//	before placing in a new unique_ptr of type ROCCoreVInterface.
				std::unique_ptr<FEVInterface> tmpVFE = makeInterface(
				    roc.second.getNode("ROCInterfacePluginName").getValue<std::string>(),
				    roc.first,
				    theXDAQContextConfigTree_,
				    (theConfigurationPath_ + "/LinkToROCGroupTable/" + roc.first));

				// setup parent supervisor of FEVinterface (for backwards compatibility,
				// left out of constructor)
				tmpVFE->parentSupervisor_ = parentSupervisor_;

				ROCCoreVInterface& tmpRoc = dynamic_cast<ROCCoreVInterface&>(
				    *tmpVFE);  // dynamic_cast<ROCCoreVInterface*>(tmpRoc.get());

				// setup other members of ROCCore (for interface plug-in compatibility,
				// left out of constructor)

				__COUTV__(tmpRoc.emulatorMode_);
				tmpRoc.emulatorMode_ = emulatorMode_;
				__COUTV__(tmpRoc.emulatorMode_);

				if(emulatorMode_)
				{
					__FE_COUT__ << "Creating ROC in emulator mode..." << __E__;

					// try
					{
						// all ROCs support emulator mode

						//						// verify ROCCoreVEmulator class
						// functionality  with  dynamic_cast
						// ROCCoreVEmulator&  tmpEmulator =
						// dynamic_cast<ROCCoreVEmulator&>(
						//						    tmpRoc);  //
						// dynamic_cast<ROCCoreVInterface*>(tmpRoc.get());

						// start emulator thread
						std::thread(
						    [](ROCCoreVInterface* rocEmulator) {
							    __COUT__ << "Starting ROC emulator thread..." << __E__;
							    ROCCoreVInterface::emulatorThread(rocEmulator);
						    },
						    &tmpRoc)
						    .detach();
					}
					//					catch(const std::bad_cast& e)
					//					{
					//						__SS__ << "Cast to ROCCoreVEmulator failed!
					// Verify  the  emulator " 						          "plugin
					// inherits
					// from  ROCCoreVEmulator."
					//						       << __E__;
					//						ss << "Failed to instantiate plugin named '"
					//<<  roc.first
					//						   << "' of type '"
					//						   <<
					// roc.second.getNode("ROCInterfacePluginName")
					//						          .getValue<std::string>()
					//						   << "' due to the following error: \n"
					//						   << e.what() << __E__;
					//
					//						__SS_THROW__;
					//					}
				}
				else
				{
					tmpRoc.thisDTC_ = thisDTC_;
				}

				rocs_.emplace(std::pair<std::string, std::unique_ptr<ROCCoreVInterface>>(
				    roc.first, &tmpRoc));
				tmpVFE.release();  // release the FEVInterface unique_ptr, so we are left
				                   // with just one

				__COUTV__(rocs_[roc.first]->emulatorMode_);
			}
			catch(const cet::exception& e)
			{
				__SS__ << "Failed to instantiate plugin named '" << roc.first
				       << "' of type '"
				       << roc.second.getNode("ROCInterfacePluginName")
				              .getValue<std::string>()
				       << "' due to the following error: \n"
				       << e.what() << __E__;
				__FE_SS_THROW__;
			}
			catch(const std::bad_cast& e)
			{
				__SS__ << "Cast to ROCCoreVInterface failed! Verify the plugin inherits "
				          "from ROCCoreVInterface."
				       << __E__;
				ss << "Failed to instantiate plugin named '" << roc.first << "' of type '"
				   << roc.second.getNode("ROCInterfacePluginName").getValue<std::string>()
				   << "' due to the following error: \n"
				   << e.what() << __E__;

				__FE_SS_THROW__;
			}
		}

	__FE_COUT__ << "Done creating " << rocs_.size() << " ROC(s)" << std::endl;
}  // end createROCs

//===============================================================================================
// registerWrite: return = value readback from register at address "address"
//	Use base class CFOandDTCCoreVInterface::registerWrite(), and do readback verification in DTCFrontEndInterface::registerWrite() and CFOFrontEndInterface::registerWrite()
dtc_data_t DTCFrontEndInterface::registerWrite(dtc_address_t address, dtc_data_t dataToWrite)
{
	dtc_data_t readbackValue = CFOandDTCCoreVInterface::registerWrite(address,dataToWrite);

	//--------------------------------------------------------
	//do DTC-specific readback verification here...


	dtc_data_t i = -1; //use for counters or mask (default mask to all 1s with -1)
	switch(address)
	{
		case 0x916c: // this is a DTC-only I2C register, it clears bit-0 when transaction finishes
			while((dataToWrite & 0x1) && (readbackValue & 0x1)) //wait for I2C to clear...
			{	
				readbackValue = registerRead(address);
				usleep(100);
				if((++i % 10) == 9)
					__FE_COUT__ << "I2C waited " << i+1 << " times..." << __E__;				
			}
			dataToWrite		&= ~1; 
			readbackValue 	&= ~1;
			break;
		case 0x91f8: //CFO emulator marker enables: 5:0 enables clock marker, 13:8 is event marker per ROC link 
				//for some reason, now event marker is not returned (FIXME?)
			dataToWrite		&= 0x03f; 
			readbackValue 	&= 0x03f;
			break;
		case 0x93c8: //clears 32-bit CDR unlock counter, but can read back errors immediately
			return readbackValue;
		case 0x9308:  //0x9308 bit-0 is reset, input select bit-5:4, bit-8 is LOL, bit-11:9 (input LOS)
			dataToWrite		&= (3<<4); 
			readbackValue 	&= (3<<4);
			break;
		default: //leverage base class for all other addresses
			CFOandDTCCoreVInterface::readbackVerify(address,dataToWrite,readbackValue);
			return readbackValue;
	} //end readback verification address case handling

	if(readbackValue != dataToWrite)
	{
		__FE_SS__ 	<< "write value 0x"	<< std::setw(8) << std::setprecision(8) << std::hex << dataToWrite
				<< " to register 0x" 	<< std::setw(4) << std::setprecision(4) << std::hex << address << 
				"... read back 0x"	 	<< std::setw(8) << std::setprecision(8) << std::hex << readbackValue <<
				"\n\n" << StringMacros::stackTrace() << __E__;
		__FE_SS_THROW__;
	}
	//end DTC-specific readback verification here...
	//--------------------------------------------------------
	

	return readbackValue;
}  // end registerWrite()

//==================================================================================================
std::string DTCFrontEndInterface::readStatus(void)
{
	std::stringstream ss;
	
	ss << device_name_ << " firmware version (0x9004) = 0x" << std::hex
	            << registerRead(0x9004) << __E__;

	ss << printVoltages() << __E__;

	ss << device_name_ << " temperature = " << readTemperature() << " degC"
	            << __E__ << __E__;

	ss << device_name_ << " SERDES reset........ (0x9118) = 0x" << std::hex
	            << registerRead(0x9118) << __E__;
	ss << device_name_ << " SERDES disparity err (0x911c) = 0x" << std::hex
	            << registerRead(0x9118) << __E__;
	ss << device_name_ << " SERDES unlock error. (0x9124) = 0x" << std::hex
	            << registerRead(0x9124) << __E__;
	ss << device_name_ << " PLL locked.......... (0x9128) = 0x" << std::hex
	            << registerRead(0x9128) << __E__;
	ss << device_name_ << " SERDES Rx status.... (0x9134) = 0x" << std::hex
	            << registerRead(0x9134) << __E__;
	ss << device_name_ << " SERDES reset done... (0x9138) = 0x" << std::hex
	            << registerRead(0x9138) << __E__;
	ss << device_name_ << " link status......... (0x9140) = 0x" << std::hex
	            << registerRead(0x9140) << __E__;
	ss << device_name_ << " SERDES ref clk freq. (0x915c) = 0x" << std::hex
	            << registerRead(0x915c) << " = " << std::dec << registerRead(0x915c)
	            << __E__;
	ss << device_name_ << " control............. (0x9100) = 0x" << std::hex
	            << registerRead(0x9100) << __E__;
	__FE_COUT__ << ss.str();

	return ss.str();
} //end readStatus()

//==================================================================================================
int DTCFrontEndInterface::getROCLinkStatus(int ROC_link)
{
	int overall_link_status = registerRead(0x9140);

	int ROC_link_status = (overall_link_status >> ROC_link) & 0x1;

	return ROC_link_status;
} //end getROCLinkStatus()

//==================================================================================================
int DTCFrontEndInterface::getCFOLinkStatus()
{
	int overall_link_status = registerRead(0x9140);

	int CFO_link_status = (overall_link_status >> 6) & 0x1;

	return CFO_link_status;
} //end getCFOLinkStatus()

//==================================================================================================
int DTCFrontEndInterface::checkLinkStatus()
{
	int ROCs_OK = 1;

	for(int i = 0; i < 8; i++)
	{
		//__FE_COUT__ << " check link " << i << " ";

		if(ROCActive(i))
		{
			//__FE_COUT__ << " active... status = " << getROCLinkStatus(i) << __E__;
			ROCs_OK &= getROCLinkStatus(i);
		}
	}

	if((getCFOLinkStatus() == 1) && ROCs_OK == 1)
	{
		//	__FE_COUT__ << "DTC links OK = 0x" << std::hex << registerRead(0x9140)
		//<< std::dec << __E__;
		//	__MOUT__ << "DTC links OK = 0x" << std::hex << registerRead(0x9140) <<
		// std::dec << __E__;

		return 1;
	}
	else
	{
		//	__FE_COUT__ << "DTC links not OK = 0x" << std::hex <<
		// registerRead(0x9140) << std::dec << __E__;
		//	__MOUT__ << "DTC links not OK = 0x" << std::hex << registerRead(0x9140)
		//<< std::dec << __E__;

		return 0;
	}
} //end checkLinkStatus()

//==================================================================================================
bool DTCFrontEndInterface::ROCActive(unsigned ROC_link)
{
	// __FE_COUTV__(roc_mask_);
	// __FE_COUTV__(ROC_link);

	if(((roc_mask_ >> ROC_link) & 0x01) == 1)
	{
		return true;
	}
	else
	{
		return false;
	}
} //end ROCActive()

//==================================================================================================
void DTCFrontEndInterface::configure(void) try
{
	__FE_COUTV__(getIterationIndex());
	__FE_COUTV__(getSubIterationIndex());

	if(regWriteMonitorStream_.is_open())
	{
		regWriteMonitorStream_ << "Timestamp: " << std::dec << time(0) << 
			", \t ---------- Configure step " << 
			getIterationIndex() << ":" << getSubIterationIndex() << "\n";
		regWriteMonitorStream_.flush();
	}


	if(getConfigurationManager()
	        ->getNode("/Mu2eGlobalsTable/SyncDemoConfig/SkipCFOandDTCConfigureSteps")
	        .getValue<bool>())
	{
		__FE_COUT_INFO__ << "Skipping configure steps!" << __E__;
		return;
	}
	
	if(emulatorMode_)
	{
		__FE_COUT__ << "Emulator DTC configuring... # of ROCs = " << rocs_.size()
		            << __E__;
		for(auto& roc : rocs_)
			roc.second->configure();
		return;
	}
	
	// From Rick new code
	uint32_t dtcEventBuilderReg_DTCID = 0;
	uint32_t dtcEventBuilderReg_Mode = 0;
	uint32_t dtcEventBuilderReg_PartitionID = 0;
	uint32_t dtcEventBuilderReg_MACIndex = 0;
	uint32_t dtcEventBuilderReg_DTCInfo = 0;

	uint32_t dtcEventBuilderReg_NumBuff = 0;
	uint32_t dtcEventBuilderReg_StartNode = 0;
	uint32_t dtcEventBuilderReg_NumNodes = 0;
    uint32_t dtcEventBuilderReg_Configuration = 0;

        try
          {
            dtcEventBuilderReg_DTCID = getSelfNode().getNode("EventBuilderDTCID").getValue<uint32_t>();
			dtcEventBuilderReg_Mode = getSelfNode().getNode("EventBuilderMode").getValue<uint32_t>();
            dtcEventBuilderReg_PartitionID = getSelfNode().getNode("EventBuilderPartitionID").getValue<uint32_t>();
			dtcEventBuilderReg_MACIndex = getSelfNode().getNode("EventBuilderMACIndex").getValue<uint32_t>();
            
			dtcEventBuilderReg_NumBuff = getSelfNode().getNode("EventBuilderNumBuff").getValue<uint32_t>();
			dtcEventBuilderReg_StartNode = getSelfNode().getNode("EventBuilderStartNode").getValue<uint32_t>();
			dtcEventBuilderReg_NumNodes = getSelfNode().getNode("EventBuilderNumNodes").getValue<uint32_t>();


            __FE_COUTV__(dtcEventBuilderReg_DTCID);  // Doesn't work if I use uint8_t
			__FE_COUTV__(dtcEventBuilderReg_Mode);
			__FE_COUTV__(dtcEventBuilderReg_PartitionID);
			__FE_COUTV__(dtcEventBuilderReg_MACIndex);
			__FE_COUTV__(dtcEventBuilderReg_NumBuff);
			__FE_COUTV__(dtcEventBuilderReg_StartNode);
			__FE_COUTV__(dtcEventBuilderReg_NumNodes);



			// Register x9154 is #DTC ID [31-24] / EVB Mode [23-16]/ EVB Partition ID [15-8]/ EVB Local MAC Index [7-0]
			dtcEventBuilderReg_DTCInfo = dtcEventBuilderReg_DTCID << 24 | dtcEventBuilderReg_Mode << 16 | 
											dtcEventBuilderReg_PartitionID << 8 | dtcEventBuilderReg_MACIndex;
			__FE_COUTV__(dtcEventBuilderReg_DTCInfo);
			registerWrite(0x9154, dtcEventBuilderReg_DTCInfo);

			// Register x9158 is #Num EVB Buffers[22-16], EVB Start Node [14-8], Num Nodes [6-0]
            dtcEventBuilderReg_Configuration = dtcEventBuilderReg_NumBuff << 16 | dtcEventBuilderReg_StartNode << 8 
												| dtcEventBuilderReg_NumNodes; 
			__FE_COUTV__(dtcEventBuilderReg_Configuration);    
            registerWrite(0x9158, dtcEventBuilderReg_Configuration);
		
          }
        catch(...)
          {
            __FE_COUT_INFO__ << "Ignoring missing event building configuration values." << __E__;
          }
	// end of the new code

	__FE_COUT__ << "DTC configuring... # of ROCs = " << rocs_.size() << __E__;

	// NOTE: otsdaq/xdaq has a soap reply timeout for state transitions.
	// Therefore, break up configuration into several steps so as to reply before
	// the time out As well, there is a specific order in which to configure the
	// links in the chain of CFO->DTC0->DTC1->...DTCN

	const int number_of_system_configs =
	    -1;  // if < 0, keep trying until links are OK.
	         // If > 0, go through configuration steps this many times

	const int  reset_fpga               = 1;                 // 1 = yes, 0 = no
	//const bool config_clock             = configure_clock_;  // 1 = yes, 0 = no
	const bool config_jitter_attenuator = configure_clock_;  // 1 = yes, 0 = no
	//const int  reset_rx                 = 0;                 // 1 = yes, 0 = no

	const int number_of_dtc_config_steps = 7; 

	const int max_number_of_tries = 3;  // max number to wait for links OK

	int number_of_total_config_steps =
	    number_of_system_configs * number_of_dtc_config_steps;

	int config_step    = getIterationIndex();
	int config_substep = getSubIterationIndex();

	bool isLastTimeThroughConfigure = false;
	

	if(number_of_system_configs > 0)
	{
		if(config_step >= number_of_total_config_steps)  // done, exit system config
			return;
	}

	// waiting for link loop
	if(config_substep > 0 && config_substep < max_number_of_tries)
	{  // wait a maximum of 30 seconds

		const int number_of_link_checks = 10;

		//int link_ok = 0;

		for(int i = 0; i < number_of_link_checks; i++)
		{
			if(checkLinkStatus() == 1)
			{
				// links OK,  continue with the rest of the configuration
				__FE_COUT__ << device_name_ << " Link Status is OK = 0x" << std::hex
				            << registerRead(0x9140) << std::dec << __E__;

				indicateIterationWork();
				turnOffLED();
				return;
			}
			else if(getCFOLinkStatus() == 0)
			{
				// in this case, links will never get to be OK, stop waiting for them

				__FE_COUT__ << device_name_ << " CFO Link Status is bad = 0x" << std::hex
				            << registerRead(0x9140) << std::dec << __E__;

				// usleep(500000 /*500ms*/); 
				sleep(1);

				indicateIterationWork();
				turnOffLED();
				return;
			}
			else
			{
				// links still not OK, keep checking...

				__FE_COUT__ << "Waiting for DTC Link Status = 0x" << std::hex
				            << registerRead(0x9140) << std::dec << __E__;
				// usleep(500000 /*500ms*/); 							
				sleep(1);
			}
		}

		indicateSubIterationWork();
		return;
	}
	else if(config_substep > max_number_of_tries)
	{
		//wait a maximum of 30 seconds, then stop waiting for them

		__FE_COUT__ << "Links still bad = 0x" << std::hex << registerRead(0x9140)
		            << std::dec << "... continue" << __E__;
		indicateIterationWork();
		turnOffLED();
		return;
	}

	turnOnLED();

	if((config_step % number_of_dtc_config_steps) == 0)
	{
		
				__FE_COUTV__(GetFirmwareVersion());
		if(reset_fpga == 1 && config_step < number_of_dtc_config_steps)
		{
			// only reset the FPGA the first time through
			__FE_COUT_INFO__ << "Step " << config_step << ": " << device_name_
			                       << " RESET FPGA... " << __E__;

			DTCReset();
		}
		
		
	}
	else if((config_step % number_of_dtc_config_steps) == 1)
	{
		
		__FE_COUT_INFO__ << "Step" << config_step << ": " << device_name_
				<< "select/setup clock..." << __E__;

		//choose jitter attenuator input select (reg 0x9308, bits 5:4)
		// 0 is Upstream Control Link Rx Recovered Clock
		// 1 is RJ45 Upstream Clock
		// 2 is Timing Card Selectable (SFP+ or FPGA) Input Clock
		{
			uint32_t readData = registerRead(0x9308);
			uint32_t val = 0;
			try
			{
				val =  getSelfNode().getNode("JitterAttenuatorInputSource").getValue<uint32_t>();
				
			}
			catch(...)
			{
				__FE_COUT__ << "Defaulting Jitter Attenuator Input Source to val = " << val << __E__;
			}
			readData &= ~(3<<4); //clear the two bits
			readData &= ~(1); //ensure unreset of jitter attenuator
			readData |= (val & 3)<<4; //set the two bits to selected value
			
			registerWrite(0x9308, readData );
			__FE_COUT__ << "Jitter Attenuator Input Select: " << val << " ==> " <<
				(val==0?
				"Upstream Control Link Rx Recovered Clock":
				(val == 1?"RJ45 Upstream Clock":"Timing Card Selectable (SFP+ or FPGA) Input Clock")) 
				<< __E__;
		}
		
		//Rick says DTC crystals should be correct by default, so removing crystal configuration

		// if((config_clock == 1 || emulate_cfo_ == 1) &&
		//    config_step < number_of_dtc_config_steps)
		// {
		// 	// only configure the clock/crystal the first loop through...

		// 	__FE_COUT_INFO__ << "Step " << config_step << ": " << device_name_
		// 	                       << " reset clock..." << __E__;
			                       
		// 	__FE_COUT__ << "DTC - set crystal frequency to 156.25 MHz" << __E__;
		// //	registerWrite(0x915c, 0x09502F90); 
		// 	registerWrite(0x9160, 0x0bebc200);

		// 	// set RST_REG bit
		// //	registerWrite(0x9168, 0x5d870100); // original, but in chants its the line below
		// 	registerWrite(0x9168, 0x55870100);
		// 	registerWrite(0x916c, 0x00000001);

		// 	//usleep(500000 /*500ms*/); 
		// 	sleep(5);

		// 	int targetFrequency = 200000000;

		// 	//--- begin code snippet pulled from: mu2eUtil program_clock -C 2 -F
		// 	// 200000000

		// 	// auto oscillator = DTCLib::DTC_OscillatorType_SERDES; //-C 0 = CFO (main
		// 	// board SERDES clock) auto oscillator = DTCLib::DTC_OscillatorType_DDR;
		// 	// //-C 1 (DDR clock)
		// 	auto oscillator =
		// 	    DTCLib::DTC_OscillatorType_Timing;  //-C 2 = DTC (with timing card)

		// 	__FE_COUT__ << "DTC - set oscillator frequency to " << std::dec
		// 	            << targetFrequency << " MHz" << __E__;

		// 	thisDTC_->SetNewOscillatorFrequency(oscillator, targetFrequency);

		// 	//--- end code snippet pulled from: mu2eUtil program_clock -C 2 -F
		// 	// 200000000

		// 	//usleep(500000 /*500ms*/); 
		// 	sleep(5);
		// }
		// else
		// {
		// 	__MCOUT_INFO__("Step " << config_step << ": " << device_name_
		// 	                       << " do NOT reset clock..." << __E__);
		// }
	}
	else if((config_step % number_of_dtc_config_steps) == 2)
	{
		// configure Jitter Attenuator to recover clock
		if((config_jitter_attenuator == 1 || emulate_cfo_ == 1) &&
		   config_step < number_of_dtc_config_steps)
		{
			__MCOUT_INFO__("Step " << config_step << ": " << device_name_
			                       << " configure Jitter Attenuator..." << __E__);

			//It's needed only after a powercycle
			configureJitterAttenuator();

			usleep(500000 /*500ms*/); 
			sleep(5);
		}
		else
		{
			__MCOUT_INFO__("Step " << config_step << ": " << device_name_
			                       << " do NOT configure Jitter Attenuator..." << __E__);
		}
	}
	else if((config_step % number_of_dtc_config_steps) == 3)
	{
		//if we skip this step the DTC doesn't lock
		// reset CFO links, first what is received from upstream, then what is
		// transmitted downstream

		if(emulate_cfo_ == 1)
		{
			__MCOUT_INFO__("Step " << config_step << ": " << device_name_
			                       << " enable CFO emulation and internal clock");
			//int dataInReg = registerRead(0x9100);
			int dataToWrite = 0x40808404;  // new incantation from Rick K.(disable retransmission) 12/18/2019m
			registerWrite(0x9100, dataToWrite);

		//	__FE_COUT__ << ".......  CFO emulation: set time between Event Windows" << __E__;
		//	registerWrite(0x91f0, 0x4e20);

			__FE_COUT__ << ".......CFO emulation: time between data requests" << __E__;
			registerWrite(0x91a8, 0x4e20);
		}
		else
		{
			int dataInReg = registerRead(0x9100);
			int dataToWrite =
			    dataInReg &
			    0xbfff7fff;  // bit 30 = CFO emulation enable; bit 15 CFO emulation mode
			registerWrite(0x9100, dataToWrite);
		}

		// removed RESET OF CFO LINK - SHOULD NOT BE NECESSARY with firmware version 20181024.
		
	}
	else if((config_step % number_of_dtc_config_steps) == 4)
	{
		/*
		__MCOUT_INFO__("Step " << config_step << ": " << device_name_
		                       << " wait for links..." << __E__);

		// removed UNRESET OF CFO LINK - SHOULD NOT BE NECESSARY with firmware version 20181024. Just return.

		indicateSubIterationWork();  // tell state machine to stay in configure state
		                             // ("come back to me")

		return;
		*/
	}
	else if((config_step % number_of_dtc_config_steps) == 5)
	{
		__MCOUT_INFO__("Step " << config_step << ": " << device_name_
		                       << " enable markers, Tx, Rx" << __E__);

		// enable markers, tx and rx

		int data_to_write = (roc_mask_ << 8) | roc_mask_;
		__FE_COUT__ << "CFO enable markers - enabled ROC links 0x" << std::hex
		            << data_to_write << std::dec << __E__;
		registerWrite(0x91f8, data_to_write);

		data_to_write = 0x4040 | (roc_mask_ << 8) | roc_mask_;
		__FE_COUT__ << "DTC enable tx and rx - CFO and enabled ROC links 0x" << std::hex
		            << data_to_write << std::dec << __E__;
		registerWrite(0x9114, data_to_write);

		data_to_write = 0x00014141; // DMA timeout from chants.
		__FE_COUT__ << "set DMA timeout" << std::hex
		            << data_to_write << std::dec << __E__;
		registerWrite(0x9144, data_to_write);

		// put DTC CFO link output into loopback mode
		__FE_COUT__ << "DTC set CFO link output loopback mode ENABLE" << __E__;


		__MCOUT_INFO__("Step " << config_step << ": " << device_name_ << " configure ROCs"
		                       << __E__);

		bool doConfigureROCs = false;
		try
		{
			doConfigureROCs = Configurable::getSelfNode()
			                      .getNode("EnableROCConfigureStep")
			                      .getValue<bool>();
		}
		catch(...)
		{
		}  // ignore missing field
		if(doConfigureROCs)
			for(auto& roc : rocs_)
				roc.second->configure();

		// usleep(500000 /*500ms*/); 
		sleep(1);
	}
	else if((config_step % number_of_dtc_config_steps) == 6)
	{
		
		if(emulate_cfo_ == 1)
		{
			__MCOUT_INFO__("Step " << config_step
			                       << ": CFO emulation enable Event start characters "
			                          "and event window interval"
			                       << __E__);

			__FE_COUT__ << "CFO emulation:  set Event Window interval" << __E__;
			registerWrite(0x91f0, 0x00000000);  // for NO markers, write these
			

			__FE_COUT__ << "CFO emulation:  set 40MHz marker interval" << __E__;
			registerWrite(0x91f4, 0x00000000);  // for NO markers, write these

			__FE_COUT__ << "CFO emulation:  set heartbeat interval " << __E__;
		}
		__MCOUT_INFO__("Step " << config_step << ": " << device_name_ << " configured"
		                       << __E__);


		__FE_COUTV__(getIterationIndex());
		__FE_COUTV__(getSubIterationIndex());

		
		if(checkLinkStatus() == 1)
		{
			__MCOUT_INFO__(device_name_ << " links OK 0x" << std::hex
			                            << registerRead(0x9140) << std::dec << __E__);

			// usleep(500000 ); //500ms/
			sleep(1);
			turnOffLED();

			if(number_of_system_configs < 0)
			{
				isLastTimeThroughConfigure = true;
				// do a final DTC Reset
	  			__MCOUT_INFO__("Last step in configuration; doing DTCReset" << __E__);
	  			DTCReset();
			}
		}
		else if(config_step > max_number_of_tries)
		{
			isLastTimeThroughConfigure = true;
			__MCOUT_INFO__(device_name_ << " after " << max_number_of_tries <<
				" tries, links not OK 0x" << std::hex
				<< registerRead(0x9140) << std::dec << __E__);						
		}
		else
		{
			__MCOUT_INFO__(device_name_ << " links not OK 0x" << std::hex
			                            << registerRead(0x9140) << std::dec << __E__);
		}

		
		__FE_COUTV__(isLastTimeThroughConfigure);
		if(isLastTimeThroughConfigure)
		{
			sleep(5);
			//write anything to reset
			//0x93c8 is RX CDR Unlock counter (32-bit)
			__MCOUT_INFO__("LAST STEP!! Reset Loss-of-Lock Counter() on DTC");

			registerWrite(0x93c8, 0); 
				
			return;  // links OK, kick out of configure OR link tries complete
		}
		
		
	}

	readStatus();  // spit out link status at every step

	indicateIterationWork();  // otherwise, tell state machine to stay in configure
	                          // state ("come back to me")

	
	turnOffLED();


	return;
}  // end configure()
catch(const std::runtime_error& e)
{
	__FE_SS__ << "Error caught: " << e.what() << __E__;
	__FE_SS_THROW__;
}
catch(...)
{
	__FE_SS__ << "Unknown error caught. Check the printouts!" << __E__;
	__FE_SS_THROW__;
}

//==============================================================================
void DTCFrontEndInterface::halt(void)
{
	if(regWriteMonitorStream_.is_open())
	{
		regWriteMonitorStream_ << "Timestamp: " << std::dec << time(0) << 
			", \t ---------- Halting..." << "\n";
		regWriteMonitorStream_.flush();
	}

	__FE_COUT__ << "Halting..." << __E__;

	for(auto& roc : rocs_)  // halt "as usual"
	{
		roc.second->halt();
	}

	rocs_.clear();

	__FE_COUT__ << "Halted." << __E__;

	// if(device_name_ == "DTC8")
	// {
	// 	__FE_COUT__ << "Forcing abort" << __E__;
	// 	abort();
	// }

	//	__FE_COUT__ << "HALT: DTC status" << __E__;
	//	readStatus();

	if(runDataFile_.is_open()) runDataFile_.close();
}  // end halt()

//==============================================================================
void DTCFrontEndInterface::pause(void)
{
	if(regWriteMonitorStream_.is_open()) 
	{
		regWriteMonitorStream_ << "Timestamp: " << std::dec << time(0) << 
			", \t ---------- Pausing..." << "\n";
		regWriteMonitorStream_.flush();
	}

	__FE_COUT__ << "Pausing..." << __E__;
	for(auto& roc : rocs_)  // pause "as usual"
	{
		roc.second->pause();
	}

	//	__FE_COUT__ << "PAUSE: DTC status" << __E__;
	//	readStatus();

	__FE_COUT__ << "Paused." << __E__;
}

//==============================================================================
void DTCFrontEndInterface::resume(void)
{
	if(regWriteMonitorStream_.is_open()) 
	{
		regWriteMonitorStream_ << "Timestamp: " << std::dec << time(0) << 
			", \t ---------- Resuming..." << "\n";
		regWriteMonitorStream_.flush();
	}

	__FE_COUT__ << "Resuming..." << __E__;
	for(auto& roc : rocs_)  // resume "as usual"
	{
		roc.second->resume();
	}

	//	__FE_COUT__ << "RESUME: DTC status" << __E__;
	//	readStatus();

	__FE_COUT__ << "Resumed." << __E__;
}  // end resume()

//==============================================================================
void DTCFrontEndInterface::start(std::string runNumber)
{
	if(regWriteMonitorStream_.is_open()) 
	{
		regWriteMonitorStream_ << "Timestamp: " << std::dec << time(0) << 
			", \t ---------- Starting..." << "\n";
		regWriteMonitorStream_.flush();
	}

	// open a file for this run number to write data to, if it hasn't been opened yet
	// define a data file 
	if(!artdaqMode_)
	{
		std::string runDataFilename = std::string(__ENV__("OTSDAQ_DATA")) + "/RunData_" + runNumber + 
			"_" + device_name_ + ".dat";

		__FE_COUTV__(runDataFilename);		
		if(runDataFile_.is_open())
		{
			__SS__ << "File was left open! How was this possible -  open data file RunData: " << runDataFilename << __E__;
			__SS_THROW__;
		}

		runDataFile_.open (runDataFilename, std::ios::out | std::ios::app);

		if (runDataFile_.fail())
		{
			__SS__ << "FAILED to open data file RunData: " << runDataFilename << __E__;
			__SS_THROW__;
		}
		
	} //end local run file creation

	if(emulatorMode_)
	{
		__FE_COUT__ << "Emulator DTC starting... # of ROCs = " << rocs_.size()
		            << __E__;
		for(auto& roc : rocs_) {
		  __FE_COUT__ << "Starting ROC ";
			roc.second->start(runNumber);
			__FE_COUT__ << "Done starting ROC";}
		return;
	}


	int numberOfLoopbacks =
	    getConfigurationManager()
	        ->getNode("/Mu2eGlobalsTable/SyncDemoConfig/NumberOfLoopbacks")
	        .getValue<unsigned int>();

	__FE_COUTV__(numberOfLoopbacks);

	//int stopIndex = getIterationIndex();

	if(numberOfLoopbacks == 0)
	{
		for(auto& roc : rocs_)  // start "as usual"
		{
			roc.second->start(runNumber);
		}
		return;
	}

	const int numberOfChains       = 1;
	//int       link[numberOfChains] = {0};

	const int numberOfDTCsPerChain = 1;

	const int numberOfROCsPerDTC = 1;  // assume these are ROC0 and ROC1

	// To do loopbacks on all CFOs, first have to setup all DTCs, then the CFO
	// (this method) work per iteration.  Loop back done on all chains (in this
	// method), assuming the following order: i  DTC0  DTC1  ...  DTCN 0  ROC0
	// none  ...  none 1  ROC1  none  ...  none 2  none  ROC0  ...  none 3  none
	// ROC1  ...  none
	// ...
	// N-1  none  none  ...  ROC0
	// N  none  none  ...  ROC1

	int totalNumberOfMeasurements =
	    numberOfChains * numberOfDTCsPerChain * numberOfROCsPerDTC;

	int loopbackIndex = getIterationIndex();

	if(loopbackIndex == 0)  // start
	{
		initial_9100_ = registerRead(0x9100);
		initial_9114_ = registerRead(0x9114);
		indicateIterationWork();
		return;
	}

	if(loopbackIndex > totalNumberOfMeasurements)  // finish
	{
		__MCOUT_INFO__(device_name_ << " loopback DONE" << __E__);

		if(checkLinkStatus() == 1)
		{
			//      __MCOUT_INFO__(device_name_ << " links OK 0x" << std::hex <<
			//      registerRead(0x9140) << std::dec << __E__);
		}
		else
		{
			//      __MCOUT_INFO__(device_name_ << " links not OK 0x" << std::hex <<
			//      registerRead(0x9140) << std::dec << __E__);
		}

		if(0)
			for(auto& roc : rocs_)
			{
				__MCOUT_INFO__(".... ROC" << roc.second->getLinkID() << "-DTC link lost "
				                          << roc.second->readDTCLinkLossCounter()
				                          << " times");
			}

		registerWrite(0x9100, initial_9100_);
		registerWrite(0x9114, initial_9114_);

		return;
	}

	//=========== Perform loopback=============

	// where are we in the procedure?
	unsigned int activeROC = (loopbackIndex - 1) % numberOfROCsPerDTC;

	int activeDTC = -1;

	for(int nDTC = 0; nDTC < numberOfDTCsPerChain; nDTC++)
	{
		if((loopbackIndex - 1) >= (nDTC * numberOfROCsPerDTC) &&
		   (loopbackIndex - 1) < ((nDTC + 1) * numberOfROCsPerDTC))
		{
			activeDTC = nDTC;
		}
	}

	// __FE_COUT__ << "loopback index = " << loopbackIndex
	// 	<< " activeDTC = " << activeDTC
	//	 	<< " activeROC = " << activeROC
	//	 	<< __E__;

	if(activeDTC == dtc_location_in_chain_)
	{
		__FE_COUT__ << "DTC" << activeDTC << "loopback mode ENABLE" << __E__;
		int dataInReg   = registerRead(0x9100);
		int dataToWrite = dataInReg & 0xefffffff;  // bit 28 = 0
		registerWrite(0x9100, dataToWrite);
	}
	else
	{
		// this DTC is lower in chain than the one being looped.  Pass the loopback
		// signal through
		__FE_COUT__ << "active DTC = " << activeDTC
		            << " is NOT this DTC = " << dtc_location_in_chain_
		            << "... pass signal through" << __E__;

		int dataInReg   = registerRead(0x9100);
		int dataToWrite = dataInReg | 0x10000000;  // bit 28 = 1
		registerWrite(0x9100, dataToWrite);
	}

	int ROCToEnable =
	    0x00004040 |
	    (0x101 << activeROC);  // enables TX and Rx to CFO (bit 6) and appropriate ROC
	__FE_COUT__ << "enable ROC " << activeROC << " --> 0x" << std::hex << ROCToEnable
	            << std::dec << __E__;

	registerWrite(0x9114, ROCToEnable);

	indicateIterationWork();  // FIXME -- go back to including the ROC (could not 'read'
	                          // for some reason)
	return;
	// Re-align the links for the activeROC
	for(auto& roc : rocs_)
	{
		if(roc.second->getLinkID() == activeROC)
		{
			__FE_COUT__ << "... ROC realign link... " << __E__;
			roc.second->writeRegister(22, 0);
			roc.second->writeRegister(22, 1);
		}
	}

	indicateIterationWork();
	return;
}  // end start()

//==============================================================================
void DTCFrontEndInterface::stop(void)
{
	if(regWriteMonitorStream_.is_open()) 
	{
		regWriteMonitorStream_ << "---------- Stopping..." << "\n";
		regWriteMonitorStream_.flush();
	}

	//must close data file on each possible return with call 'if(runDataFile_.is_open()) runDataFile_.close();'

	if(emulatorMode_)
    {
		__FE_COUT__ << "Emulator DTC stopping... # of ROCs = " << rocs_.size()
			<< __E__;
		for(auto& roc : rocs_)
			roc.second->stop();

		if(runDataFile_.is_open()) runDataFile_.close();
		return;
    }




	int numberOfCAPTANPulses =
	    getConfigurationManager()
	        ->getNode("/Mu2eGlobalsTable/SyncDemoConfig/NumberOfCAPTANPulses")
	        .getValue<unsigned int>();

	__FE_COUTV__(numberOfCAPTANPulses);

	int stopIndex = getIterationIndex();

	if(numberOfCAPTANPulses == 0)
	{
		for(auto& roc : rocs_)  // stop "as usual"
		{
			roc.second->stop();
		}
		if(runDataFile_.is_open()) runDataFile_.close();
		return;
	}

	if(stopIndex == 0)
	{
//		int i = 0;
		for(auto& roc : rocs_)
		{
			// re-align link
			roc.second->writeRegister(22, 0);
			roc.second->writeRegister(22, 1);

			//std::stringstream filename;
			//filename << "/home/mu2edaq/sync_demo/ots/" << device_name_ << "_ROC"
			//         << roc.second->getLinkID() << "data.txt";
			//std::string filenamestring = filename.str();
			//datafile_[i].open(filenamestring);
		//	i++;
		}
	}

	if(stopIndex > numberOfCAPTANPulses)
	{
		int i = 0;
		for(auto& roc : rocs_)
		{
			__MCOUT_INFO__(".... ROC" << roc.second->getLinkID() << "-DTC link lost "
			                          << roc.second->readDTCLinkLossCounter()
			                          << " times");
			datafile_[i].close();
			i++;
		}
		if(runDataFile_.is_open()) runDataFile_.close();
		return;
	}

	int i = 0;
	__FE_COUT__ << "Entering read timestamp loop..." << __E__;
	for(auto& roc : rocs_)
	{
		int timestamp_data = roc.second->readTimestamp();

		__FE_COUT__ << "Read " << stopIndex << " -> " << device_name_ << " timestamp "
		            << timestamp_data << __E__;

		datafile_[i] << stopIndex << " " << timestamp_data << std::endl;
		i++;
	}

	indicateIterationWork();
	return;
}  // end stop()

//==============================================================================
//return true to keep running
bool DTCFrontEndInterface::running(void)
{

//   if(artdaqMode_) {
//     __FE_COUT__ << "Running in artdaqmode" << __E__;
     return true;
//   }
	if(emulatorMode_)
	{
		__FE_COUT__ << "Emulator DTC running... # of ROCs = " << rocs_.size()
		            << __E__;
		bool stillRunning = false;
		for(auto& roc : rocs_)
			stillRunning = stillRunning || roc.second->running();
		
		return stillRunning;
	}

  // first setup DTC and CFO.  This is stolen from "getheartbeatanddatarequest"

	//	auto start = DTCLib::DTC_EventWindowTag(static_cast<uint64_t>(timestampStart));


    //    std::time_t current_time;	

	bool     incrementTimestamp = true;

	uint32_t cfodelay = 10000;  // have no idea what this is, but 1000 didn't work (don't
	                            // know if 10000 works, either)
	int requestsAhead = 0;
	unsigned int number = -1; // largest number of events?
	unsigned int timestampStart = 0;

	auto device = thisDTC_->GetDevice();
	//auto initTime = device->GetDeviceTime();
	device->ResetDeviceTime();
	//auto afterInit = std::chrono::steady_clock::now();

	
	if(emulate_cfo_ == 1)
	{
		registerWrite(0x9100, 0x40008404);  // bit 30 = CFO emulation enable, bit 15 = CFO
		                                    // emulation mode, bit 2 = DCS enable
		                                    // bit 10 turns off retry which isn't working right now
		sleep(1);

		// set number of null heartbeats
		// registerWrite(0x91BC, 0x0);
		registerWrite(0x91BC, 0x10);  // new incantaton from Rick K. 12/18/2019
		//	  sleep(1);

		//# Send data
		//#disable 40mhz marker
		registerWrite(0x91f4, 0x0);
		//	  sleep(1);

		//#set num dtcs
		registerWrite(0x9158, 0x1);
		//	  sleep(1);

		bool     useSWCFOEmulator   = true;
		uint16_t debugPacketCount = 0;
		auto     debugType        = DTCLib::DTC_DebugType_SpecialSequence;
		bool     stickyDebugType  = true;
		bool     quiet            = false;
		bool     asyncRR          = false;
		bool     forceNoDebugMode = true;

		DTCLib::DTCSoftwareCFO* EmulatedCFO_ =
		    new DTCLib::DTCSoftwareCFO(thisDTC_,
		                               useSWCFOEmulator,
		                               debugPacketCount,
		                               debugType,
		                               stickyDebugType,
		                               quiet,
		                               asyncRR,
		                               forceNoDebugMode);

		EmulatedCFO_->SendRequestsForRange(
		    number,
		    DTCLib::DTC_EventWindowTag(static_cast<uint64_t>(timestampStart)),
		    incrementTimestamp,
		    cfodelay,
		    requestsAhead);

		//auto readoutRequestTime = device->GetDeviceTime();
		device->ResetDeviceTime();
		//auto afterRequests = std::chrono::steady_clock::now();

	}

	while(WorkLoop::continueWorkLoop_)
	{
		registerWrite(0x9100, 0x40008404);  // bit 30 = CFO emulation enable, bit 15 = CFO
		                                    // emulation mode, bit 2 = DCS enable
		                                    // bit 10 turns off retry which isn't working right now
		sleep(1);

		// set number of null heartbeats
		// registerWrite(0x91BC, 0x0);
		registerWrite(0x91BC, 0x10);  // new incantaton from Rick K. 12/18/2019
		//	  sleep(1);

		//# Send data
		//#disable 40mhz marker
		registerWrite(0x91f4, 0x0);
		//	  sleep(1);

		//#set num dtcs
		registerWrite(0x9158, 0x1);
		//	  sleep(1);

		bool     useSWCFOEmulator   = true;
		uint16_t debugPacketCount = 0;
		auto     debugType        = DTCLib::DTC_DebugType_SpecialSequence;
		bool     stickyDebugType  = true;
		bool     quiet            = false;
		bool     asyncRR          = false;
		bool     forceNoDebugMode = true;

		DTCLib::DTCSoftwareCFO* EmulatedCFO_ =
		    new DTCLib::DTCSoftwareCFO(thisDTC_,
		                               useSWCFOEmulator,
		                               debugPacketCount,
		                               debugType,
		                               stickyDebugType,
		                               quiet,
		                               asyncRR,
		                               forceNoDebugMode);

		EmulatedCFO_->SendRequestsForRange(
		    number,
		    DTCLib::DTC_EventWindowTag(static_cast<uint64_t>(timestampStart)),
		    incrementTimestamp,
		    cfodelay,
		    requestsAhead);

		//auto readoutRequestTime = device->GetDeviceTime();
		device->ResetDeviceTime();
		//auto afterRequests = std::chrono::steady_clock::now();

	}

		while(WorkLoop::continueWorkLoop_)
		{
		for(auto& roc : rocs_)
		{
			roc.second->running();
		}

		// print out stuff
		unsigned quietCount = 20;
		bool quiet       = false;

		std::stringstream ostr;
		ostr << std::endl;


		//		std::cout << "Buffer Read " << std::dec << ii << std::endl;
		mu2e_databuff_t* buffer;
		auto             tmo_ms = 1500;
		__FE_COUT__ << "util - before read for DAQ in running";
		auto sts = device->read_data(
		DTC_DMA_Engine_DAQ, reinterpret_cast<void**>(&buffer), tmo_ms);
		__FE_COUT__ << "util - after read for DAQ in running " << " sts=" << sts
			          << ", buffer=" << (void*)buffer;

		if(sts > 0)
		  {
		    void* readPtr = &buffer[0];
		    auto  bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
		    readPtr       = static_cast<uint8_t*>(readPtr) + 8;

		    __FE_COUT__ << "Buffer reports DMA size of " << std::dec << bufSize
			      << " bytes. Device driver reports read of " << sts << " bytes,"
			      << std::endl;

		    __FE_COUT__ << "util - bufSize is " << bufSize;
		    outputStream.write(static_cast<char*>(readPtr), sts - 8);
		    auto maxLine = static_cast<unsigned>(ceil((sts - 8) / 16.0));
		    __FE_COUT__ << "maxLine " << maxLine;
		    for(unsigned line = 0; line < maxLine; ++line)
		      {
			ostr << "0x" << std::hex << std::setw(5) << std::setfill('0') << line
			     << "0: ";
			for(unsigned byte = 0; byte < 8; ++byte)
			  {
			    if(line * 16 + 2 * byte < sts - 8u)
			      {
				auto thisWord =
				  reinterpret_cast<uint16_t*>(buffer)[4 + line * 8 + byte];
				ostr << std::setw(4) << static_cast<int>(thisWord) << " ";
			      }
			  }

		  	ostr << std::endl;
			//	std::cout << ostr.str();

			//     __SET_ARG_OUT__("readData", ostr.str());  // write to data file

			// don't write data to the log file, only the data file
			// __FE_COUT__ << ostr.str();  
	    
			if(maxLine > quietCount * 2 && quiet && line == (quietCount - 1))
			  {
			    line = static_cast<unsigned>(ceil((sts - 8) / 16.0)) -
			      (1 + quietCount);
			  }
		      }
		  }
		device->read_release(DTC_DMA_Engine_DAQ, 1);
       
		ostr << std::endl; 
    

		if (runDataFile_.is_open())
		{
			runDataFile_ << ostr.str();
			runDataFile_.flush(); // flush to disk
		}
		//__FE_COUT__ << ostr.str(); 


		delete EmulatedCFO_; 

		break;
		  }
	return true;
} //end running()

//==============================================================================
// rocRead
void DTCFrontEndInterface::ReadROC(__ARGS__)
{
	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	DTCLib::DTC_Link_ID rocLinkIndex =
	    DTCLib::DTC_Link_ID(__GET_ARG_IN__("rocLinkIndex", uint8_t));
	uint8_t address = __GET_ARG_IN__("address", uint8_t);
	__FE_COUTV__(rocLinkIndex);
	__FE_COUTV__((unsigned int)address);

	uint16_t readData = -999;

	for(auto& roc : rocs_)
	{
		__FE_COUT__ << "Found link ID " << roc.second->getLinkID() << " looking for "
		            << rocLinkIndex << __E__;

		if(rocLinkIndex == roc.second->getLinkID())
		{
		  // readData = roc.second->readRegister(address);

		  readData = thisDTC_->ReadROCRegister(rocLinkIndex, address,100);
			
			  char readDataStr[100];
			  sprintf(readDataStr,"0x%X",readData);
			  __SET_ARG_OUT__("readData",readDataStr);
			//__SET_ARG_OUT__("readData", readData);

			// for(auto &argOut:argsOut)
			__FE_COUT__ << "readData"
			            << ": " << std::hex << readData << std::dec << __E__;
			__FE_COUT__ << "End of Data";
			return;
		}
	}

	__FE_SS__ << "ROC link ID " << rocLinkIndex << " not found!" << __E__;
	__FE_SS_THROW__;

}  // end ReadROC()

//==============================================================================
// DTCStatus
//	FEMacro 'DTCStatus' generated, Oct-22-2018 03:16:46, by 'admin' using
// MacroMaker. 	Macro Notes:
void DTCFrontEndInterface::WriteROC(__ARGS__)
{
	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	DTCLib::DTC_Link_ID rocLinkIndex =
	    DTCLib::DTC_Link_ID(__GET_ARG_IN__("rocLinkIndex", uint8_t));
	uint8_t  address   = __GET_ARG_IN__("address", uint8_t);
	uint16_t writeData = __GET_ARG_IN__("writeData", uint16_t);

	__FE_COUTV__(rocLinkIndex);
	__FE_COUTV__((unsigned int)address);
	__FE_COUTV__(writeData);

	__FE_COUT__ << "ROCs size = " << rocs_.size() << __E__;

	for(auto& roc : rocs_)
	{
		__FE_COUT__ << "Found link ID " << roc.second->getLinkID() << " looking for "
		            << rocLinkIndex << __E__;

		if(rocLinkIndex == roc.second->getLinkID())
		{
			roc.second->writeRegister(address, writeData);

			for(auto& argOut : argsOut)
				__FE_COUT__ << argOut.first << ": " << argOut.second << __E__;

			return;
		}
	}

	__FE_SS__ << "ROC link ID " << rocLinkIndex << " not found!" << __E__;
	__FE_SS_THROW__;
} //end WriteROC()

//==============================================================================
void DTCFrontEndInterface::WriteROCBlock(__ARGS__)
{
	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	// macro commands section

	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;

	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	DTCLib::DTC_Link_ID rocLinkIndex =
	    DTCLib::DTC_Link_ID(__GET_ARG_IN__("rocLinkIndex", uint8_t));
	uint8_t  address   = __GET_ARG_IN__("address", uint8_t);
	uint16_t writeData = __GET_ARG_IN__("writeData", uint16_t);
	uint8_t  block     = __GET_ARG_IN__("block", uint8_t);
	__FE_COUTV__(rocLinkIndex);
	__FE_COUT__ << "block = " << std::dec << (unsigned int)block << __E__;
	__FE_COUT__ << "address = 0x" << std::hex << (unsigned int)address << std::dec
	            << __E__;
	__FE_COUT__ << "writeData = 0x" << std::hex << writeData << std::dec << __E__;

	bool acknowledge_request = false;

	thisDTC_->WriteExtROCRegister(
				      rocLinkIndex, block, address, writeData, acknowledge_request, 0);

	for(auto& argOut : argsOut)
		__FE_COUT__ << argOut.first << ": " << argOut.second << __E__;
} //end WriteROCBlock()

//==============================================================================
void DTCFrontEndInterface::BlockReadROC(__ARGS__)
{
	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	DTCLib::DTC_Link_ID rocLinkIndex =
	    DTCLib::DTC_Link_ID(__GET_ARG_IN__("rocLinkIndex", uint8_t));
	uint8_t address = __GET_ARG_IN__("address", uint8_t);
	uint8_t block   = __GET_ARG_IN__("block", uint8_t);
	__FE_COUTV__(rocLinkIndex);
	__FE_COUT__ << "block = " << std::dec << (unsigned int)block << __E__;
	__FE_COUT__ << "address = 0x" << std::hex << (unsigned int)address << std::dec
	            << __E__;

	//bool acknowledge_request = false;

	for(auto& roc : rocs_)
	{
		__FE_COUT__ << "At ROC link ID " << roc.second->getLinkID() << ", looking for "
		            << rocLinkIndex << __E__;

		if(rocLinkIndex == roc.second->getLinkID())
		{
			uint16_t readData;

			readData = thisDTC_->ReadExtROCRegister(rocLinkIndex, block, address);

			std::string readDataString = "";
			readDataString = BinaryStringMacros::binaryNumberToHexString(readData);

			// StringMacros::vectorToString(readData);

			__SET_ARG_OUT__("readData", readDataString);

			// for(auto &argOut:argsOut)
			__FE_COUT__ << "readData"
			            << ": " << readDataString << __E__;
			return;
		}
	}

	__FE_SS__ << "ROC link ID " << rocLinkIndex << " not found!" << __E__;
	__FE_SS_THROW__;
} //end BlockReadROC()

//========================================================================
void DTCFrontEndInterface::ReadROCBlock(__ARGS__)
{
	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	// macro commands section
	__FE_COUT__ << "# of input args = " << argsIn.size() << __E__;
	__FE_COUT__ << "# of output args = " << argsOut.size() << __E__;

	for(auto& argIn : argsIn)
		__FE_COUT__ << argIn.first << ": " << argIn.second << __E__;

	DTCLib::DTC_Link_ID rocLinkIndex =
	    DTCLib::DTC_Link_ID(__GET_ARG_IN__("rocLinkIndex", uint8_t));
	uint16_t address          = __GET_ARG_IN__("address", uint16_t);
	uint16_t wordCount        = __GET_ARG_IN__("numberOfWords", uint16_t);
	bool     incrementAddress = __GET_ARG_IN__("incrementAddress", bool);

	__FE_COUTV__(rocLinkIndex);
	__FE_COUT__ << "address = 0x" << std::hex << (unsigned int)address << std::dec
	            << __E__;
	__FE_COUT__ << "numberOfWords = " << std::dec << (unsigned int)wordCount << __E__;
	__FE_COUTV__(incrementAddress);

	for(auto& roc : rocs_)
	{
		__FE_COUT__ << "At ROC link ID " << roc.second->getLinkID() << ", looking for "
		            << rocLinkIndex << __E__;

		if(rocLinkIndex == roc.second->getLinkID())
		{
			std::vector<uint16_t> readData;

			roc.second->readBlock(readData, address, wordCount, incrementAddress);

			std::string readDataString = "";
			{
				bool first = true;
				for(const auto& data : readData)
				{
					if(!first)
						readDataString += ", ";
					else
						first = false;
					readDataString += BinaryStringMacros::binaryNumberToHexString(data);
				}
			}
			// StringMacros::vectorToString(readData);

			__SET_ARG_OUT__("readData", readDataString);

			// for(auto &argOut:argsOut)
			__FE_COUT__ << "readData"
			            << ": " << readDataString << __E__;
			return;
		}
	}

	__FE_SS__ << "ROC link ID " << rocLinkIndex << " not found!" << __E__;
	__FE_SS_THROW__;

}  // end ReadROCBlock()

//========================================================================
void DTCFrontEndInterface::DTCHighRateBlockCheck(__ARGS__)
{
	unsigned int linkIndex   = __GET_ARG_IN__("rocLinkIndex", unsigned int);
	unsigned int loops       = __GET_ARG_IN__("loops", unsigned int);
	unsigned int baseAddress = __GET_ARG_IN__("baseAddress", unsigned int);
	unsigned int correctRegisterValue0 =
	    __GET_ARG_IN__("correctRegisterValue0", unsigned int);
	unsigned int correctRegisterValue1 =
	    __GET_ARG_IN__("correctRegisterValue1", unsigned int);

	__FE_COUTV__(linkIndex);
	__FE_COUTV__(loops);
	__FE_COUTV__(baseAddress);
	__FE_COUTV__(correctRegisterValue0);
	__FE_COUTV__(correctRegisterValue1);

	for(auto& roc : rocs_)
		if(roc.second->getLinkID() == linkIndex)
		{
			roc.second->highRateBlockCheck(
			    loops, baseAddress, correctRegisterValue0, correctRegisterValue1);
			return;
		}

	__FE_SS__ << "Error! Could not find ROC at link index " << linkIndex << __E__;
	__FE_SS_THROW__;

}  // end DTCHighRateBlockCheck()

//========================================================================
void DTCFrontEndInterface::DTCHighRateDCSCheck(__ARGS__)
{
	unsigned int linkIndex   = __GET_ARG_IN__("rocLinkIndex", unsigned int);
	unsigned int loops       = __GET_ARG_IN__("loops", unsigned int);
	unsigned int baseAddress = __GET_ARG_IN__("baseAddress", unsigned int);
	unsigned int correctRegisterValue0 =
	    __GET_ARG_IN__("correctRegisterValue0", unsigned int);
	unsigned int correctRegisterValue1 =
	    __GET_ARG_IN__("correctRegisterValue1", unsigned int);

	__FE_COUTV__(linkIndex);
	__FE_COUTV__(loops);
	__FE_COUTV__(baseAddress);
	__FE_COUTV__(correctRegisterValue0);
	__FE_COUTV__(correctRegisterValue1);

	for(auto& roc : rocs_)
		if(roc.second->getLinkID() == linkIndex)
		{
			roc.second->highRateCheck(
			    loops, baseAddress, correctRegisterValue0, correctRegisterValue1);
			return;
		}

	__FE_SS__ << "Error! Could not find ROC at link index " << linkIndex << __E__;
	__FE_SS_THROW__;

}  // end DTCHighRateDCSCheck()

//========================================================================
void DTCFrontEndInterface::DTCSendHeartbeatAndDataRequest(__ARGS__)
{
	unsigned int number         = __GET_ARG_IN__("numberOfRequests", unsigned int);
	unsigned int timestampStart = __GET_ARG_IN__("timestampStart", unsigned int);
	bool useSWCFOEmulator 		= __GET_ARG_IN__("useSWCFOEmulator", bool);
        unsigned int rocMask        = __GET_ARG_IN__("rocMask", unsigned int);

	//	auto start = DTCLib::DTC_EventWindowTag(static_cast<uint64_t>(timestampStart));

	bool     incrementTimestamp = true;
	uint32_t cfodelay = 20000;  // have no idea what this is, but 1000 didn't work (don't
	                            // know if 10000 works, either)
	int requestsAhead = 0;
	int heartbeatPackets = 32;

	__FE_COUTV__(number);
	__FE_COUTV__(timestampStart);
	__FE_COUTV__(useSWCFOEmulator);
        __FE_COUTV__(rocMask);

	if(thisDTC_) delete thisDTC_;
	thisDTC_ = new DTCLib::DTC(DTCLib::DTC_SimMode_NoCFO, device_, rocMask,"" );
	auto device = thisDTC_->GetDevice();

	//auto initTime = device->GetDeviceTime();
	device->ResetDeviceTime();
	//auto afterInit = std::chrono::steady_clock::now();

	if(emulate_cfo_ == 1)
	{

	  thisDTC_->SetSequenceNumberDisable(); // Set 9100 bit 10
	  //registerWrite(0x9100, 0x40808404);  // bit 30 = CFO emulation enable, bit 15 = CFO
		                                    // emulation mode, bit 2 = DCS enable
		                                    // bit 10 turns off retry which isn't working right now
	  //	sleep(1);

		// set number of null heartbeats
		// registerWrite(0x91BC, 0x0);
	  //	registerWrite(0x91BC, 0x10);  // new incantaton from Rick K. 12/18/2019
		//	  sleep(1);

		//# Send data
		//#disable 40mhz marker
		registerWrite(0x91f4, 0x0);
		//	  sleep(1);

		//#set num dtcs
		registerWrite(0x9158, 0x1);
		//	  sleep(1);

		uint16_t debugPacketCount = 0;
		auto     debugType        = DTCLib::DTC_DebugType_SpecialSequence;
		bool     stickyDebugType  = true;
		bool     quiet            = false;
		bool     asyncRR          = false;
		bool     forceNoDebugMode = true;


		DTCLib::DTCSoftwareCFO* EmulatedCFO_ =
		    new DTCLib::DTCSoftwareCFO(thisDTC_,
		                               useSWCFOEmulator,
		                               debugPacketCount,
		                               debugType,
		                               stickyDebugType,
		                               quiet,
		                               asyncRR,
		                               forceNoDebugMode);

		EmulatedCFO_->SendRequestsForRange(  
		    number,
		    DTCLib::DTC_EventWindowTag(static_cast<uint64_t>(timestampStart)),
		    incrementTimestamp,
		    cfodelay,
		    requestsAhead, heartbeatPackets);


		//auto readoutRequestTime = device->GetDeviceTime();
		device->ResetDeviceTime();
		//auto afterRequests = std::chrono::steady_clock::now();

		// print out stuff
		unsigned quietCount = 20;
		quiet               = false;

		std::stringstream ostr;
		ostr << std::endl;

		for(unsigned ii = 0; ii < number; ++ii)
		{
			__FE_COUT__ << "Buffer Read " << std::dec << ii << std::endl;
			mu2e_databuff_t* buffer;
			auto             tmo_ms = 1500;
			__FE_COUT__ << "util - before read for DAQ - ii=" << ii;
			auto sts = device->read_data(
			    DTC_DMA_Engine_DAQ, reinterpret_cast<void**>(&buffer), tmo_ms);
			__FE_COUT__ << "util - after read for DAQ - ii=" << ii << ", sts=" << sts
			          << ", buffer=" << (void*)buffer;

			if(sts > 0)
			{
				void* readPtr = &buffer[0];
				auto  bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
				readPtr       = static_cast<uint8_t*>(readPtr) + 8;

				std::cout << "Buffer reports DMA size of " << std::dec << bufSize
				          << " bytes. Device driver reports read of " << sts << " bytes,"
				          << std::endl;

				std::cout << "util - bufSize is " << bufSize;
				//	__SET_ARG_OUT__("bufSize", bufSize);

				//	__FE_COUT__ << "bufSize" << bufSize;
				outputStream.write(static_cast<char*>(readPtr), sts - 8);

				auto maxLine = static_cast<unsigned>(ceil((sts - 8) / 16.0));
				std::cout << "maxLine " << maxLine;
				for(unsigned line = 0; line < maxLine; ++line)
				{
		   		        ostr << "0x" << std::hex << std::setw(5) << std::setfill('0') << line
					     << "0: ";
					for(unsigned byte = 0; byte < 8; ++byte)
					{
						if(line * 16 + 2 * byte < sts - 8u)
						{
							auto thisWord =
							    reinterpret_cast<uint16_t*>(buffer)[4 + line * 8 + byte];
							ostr << std::setw(4) << static_cast<int>(thisWord) << " ";
						}
					}

					ostr << std::endl;
					//	std::cout << ostr.str();

	        
					
	    				//__SET_ARG_OUT__("readData", ostr.str());  // write to data file

					__FE_COUT__ << ostr.str();  // write to log file

					if(maxLine > quietCount * 2 && quiet && line == (quietCount - 1))
					{
						line = static_cast<unsigned>(ceil((sts - 8) / 16.0)) -
						       (1 + quietCount);
					}
				}
			}
			device->read_release(DTC_DMA_Engine_DAQ, 1);
       
		}
		ostr << std::endl; 
    
		__SET_ARG_OUT__("readData", ostr.str()); // write to data file

		__FE_COUT__ << ostr.str(); // write to log file

		delete EmulatedCFO_; 


	}
	else
	{
		__FE_SS__ << "Error! DTC must be in CFOEmulate mode" << __E__;
		__FE_SS_THROW__;
	}
	

}  // end DTCSendHeartbeatAndDataRequest()

//========================================================================
void DTCFrontEndInterface::ResetLossOfLockCounter(__ARGS__)
{
	//write anything to reset
	//0x93c8 is RX CDR Unlock counter (32-bit)
	registerWrite(0x93c8, 0); 

	//now check
	uint32_t readData = registerRead(0x93c8); 
	
	char readDataStr[100];
	sprintf(readDataStr,"%d",readData);
	__SET_ARG_OUT__("Upstream Rx Lock Loss Count",readDataStr);
} //end ResetLossOfLockCounter()

//========================================================================
void DTCFrontEndInterface::ReadLossOfLockCounter(__ARGS__)
{
	//0x93c8 is RX CDR Unlock counter (32-bit)
	uint32_t readData = registerRead(0x93c8); 
	
	char readDataStr[100];
	sprintf(readDataStr,"%d",readData);

	//0x9140 bit-6 is RX CDR is locked
	
	bool isUpstreamLocked = 1;
	for(int i=0;i<5;++i)
	{
		readData = registerRead(0x9140); 
	 	isUpstreamLocked &= (readData >> 6)&1; //& to force unlocked for any unlocked reading
	}
	//__SET_ARG_OUT__("Upstream Rx CDR Lock Status",isUpstreamLocked?"LOCKED":"Not Locked");

	//0x9128 bit-6 is RX PLL
	readData = registerRead(0x9128); 
	bool isUpstreamPLLLocked = (readData >> 6)&1;

	//Jitter attenuator has configurable "Free Running" mode
	//LOL == Loss of Lock, LOS == Loss of Signal (4-inputs to jitter attenuator)
	//0x9308 bit-0 is reset, input select bit-5:4, bit-8 is LOL, bit-11:9 (input LOS)
	readData = registerRead(0x9308); 
	uint32_t val = (readData >> 4)&3;
	std::string JAsrc = val==0?
		"Control Link":
		(val == 1?"RJ45":"FMC/SFP+");


	__SET_ARG_OUT__("Upstream Rx Lock Loss Count",
		std::string(readDataStr) +
		"... CDR = " +
		std::string(isUpstreamLocked?" LOCKED":" Not Locked") +
		"... PLL = " +
		std::string(isUpstreamPLLLocked?" LOCKED":" Not Locked") +
		"... JA = " + JAsrc);

} //end ReadLossOfLockCounter()

//========================================================================
void DTCFrontEndInterface::GetUpstreamControlLinkStatus(__ARGS__)
{
	
	//0x93c8 is RX CDR Unlock counter (32-bit)
	uint32_t readData = registerRead(0x93c8); 
	
	char readDataStr[100];
	sprintf(readDataStr,"%d",readData);
	__SET_ARG_OUT__("Upstream Rx Lock Loss Count",readDataStr);
	
	//0x9140 bit-6 is RX CDR is locked
	bool isUpstreamLocked = 1;
	for(int i=0;i<5;++i)
	{
		readData = registerRead(0x9140); 
	 	isUpstreamLocked &= (readData >> 6)&1; //& to force unlocked for any unlocked reading
	}
	__SET_ARG_OUT__("Upstream Rx CDR Lock Status",isUpstreamLocked?"LOCKED":"Not Locked");
	
	//0x9128 bit-6 is RX PLL
	readData = registerRead(0x9128); 
	isUpstreamLocked = (readData >> 6)&1;
	__SET_ARG_OUT__("Upstream Rx PLL Lock Status",isUpstreamLocked?"LOCKED":"Not Locked");
	
	//Jitter attenuator has configurable "Free Running" mode
	//LOL == Loss of Lock, LOS == Loss of Signal (4-inputs to jitter attenuator)
	//0x9308 bit-0 is reset, input select bit-5:4, bit-8 is LOL, bit-11:9 (input LOS)
	readData = registerRead(0x9308); 
	uint32_t val = (readData >> 0)&1;
	__SET_ARG_OUT__("Jitter Attenuator Reset",val?"Held in RESET":"OK"); //OK = not in reset
	val = (readData >> 4)&3;
	__SET_ARG_OUT__("Jitter Attenuator Input Select",val==0?
		"Upstream Control Link Rx Recovered Clock":
		(val == 1?"RJ45 Upstream Clock":"Timing Card Selectable (SFP+ or FPGA) Input Clock"));
				
	std::stringstream los;
	val = (readData >> 9)&7;
	los << "...below <br><br>Loss-of-Lock: " << (((readData>>8)&1)?"Not Locked":"LOCKED");
	sprintf(readDataStr,"%X",((readData>>8) & 0x0FF));
	los << "<br>  Raw data: 0x" << std::hex << readDataStr << " = " << ((readData>>8) & 0x0FF) << std::dec << " ...";
	los << "<br><br>  Upstream Control Link Rx Recovered Clock (" << (((val>>0)&1)?"MISSING":"OK");
	los << "), \nRJ45 Upstream Rx Clock (" << (((val>>1)&1)?"MISSING":"OK");
	los << "), \nTiming Card Selectable, SFP+ or FPGA, Input Clock (" << (((val>>2)&1)?"MISSING":"OK");
	los << ")";
	__SET_ARG_OUT__("Jitter Attenuator Loss-of-Signal",los.str());
	
	//0x9138 Reset Done register
	//	TX reset done bit-7:0, 
	//	TX FSM IP reset done bit-15:8,
	//	RX reset done bit-23:16,
	//	RX FSM IP reset done bit-31:24
	val= registerRead(0x9138); 
	std::stringstream rd;
	rd << "TX reset done (" << (((val>>6)&1)?"DONE":"Not done");
	rd << "), \nTX FSM IP reset done (" << (((val>>14)&1)?"DONE":"Not done");
	rd << "), \nRX reset done (" << (((val>>22)&1)?"DONE":"Not done");
	rd << "), \nRX FSM IP reset done (" << (((val>>30)&1)?"DONE":"Not done");
	rd << ")";	
	__SET_ARG_OUT__("Reset Done",rd.str());
	
} //end GetUpstreamControlLinkStatus()


//========================================================================
void DTCFrontEndInterface::SelectJitterAttenuatorSource(__ARGS__)
{	
	uint32_t select = __GET_ARG_IN__("Source (0 is Control Link Rx, 1 is RJ45, 2 is FPGA FMC)", uint32_t);
	select %= 4;
	__FE_COUTV__((unsigned int)select);

	//choose jitter attenuator input select (reg 0x9308, bits 5:4)
	// 0 is Upstream Control Link Rx Recovered Clock
	// 1 is RJ45 Upstream Clock
	// 2 is Timing Card Selectable (SFP+ or FPGA) Input Clock

	char reg_0x9308[100];
	uint32_t val = registerRead(0x9308);
	std::stringstream results;

	sprintf(reg_0x9308,"0x%8.8X",val); __FE_COUTV__(reg_0x9308);
	results << "reg_0x9118 Starting Value: " << reg_0x9308 << __E__;

	val &= ~(3<<4); //clear the two bits
	val &= ~(1); //ensure unreset of jitter attenuator
	val |= (select & 3)<<4; //set the two bits to selected value

	sprintf(reg_0x9308,"0x%8.8X",val); __FE_COUTV__(reg_0x9308);
	results << "reg_0x9118 Select Value: " << reg_0x9308 << __E__;

	registerWrite(0x9308, val);  // write select value

	sleep(1);

	configureJitterAttenuator();

	__SET_ARG_OUT__("Register Write Results",results.str());
	__FE_COUT__ << "Done with jitter attenuator source select: " << select << __E__;

} //end SelectJitterAttenuatorSource()


//========================================================================
void DTCFrontEndInterface::WriteDTC(__ARGS__)
{	
	uint32_t address = __GET_ARG_IN__("address", uint32_t);
	uint32_t writeData = __GET_ARG_IN__("writeData", uint32_t);
	__FE_COUTV__((unsigned int)address);
	__FE_COUTV__((unsigned int)writeData);
	registerWrite(address, writeData);  
} //end WriteDTC()

//========================================================================
void DTCFrontEndInterface::ReadDTC(__ARGS__)
{	
	uint32_t address = __GET_ARG_IN__("address", uint32_t);
	__FE_COUTV__((unsigned int)address);
	uint32_t readData = registerRead(address);  
	
	char readDataStr[100];
	sprintf(readDataStr,"0x%X",readData);
	__SET_ARG_OUT__("readData",readDataStr);
} //end ReadDTC()

//========================================================================
void DTCFrontEndInterface::DTCReset(__ARGS__) { DTCReset(); }

//========================================================================
void DTCFrontEndInterface::DTCReset()
{
	__FE_COUT__ << "Starting DTC Reset..." << __E__;
	/* 
		Here are the chants:

	my_cntl write 0x9100 0x80000000  >/dev/null # reset DTC  reset serdes osc
	my_cntl write 0x9100 0x00008000 > /dev/null # Turn on CFO Emulation Mode for Serdes Reset
	my_cntl write 0x9118 0ffff00ff  >/dev/null  # SERDES resets
	my_cntl write 0x9118 0x00000000  >/dev/null  # clear SERDES reset on link 0

	sleep 1


	echo "SERDES Reset Done after reset: "
	my_cntl read 0x9138

	*/

	//reset DTC serdes osc
	registerWrite(0x9100, registerRead(0x9100) | (1<<31)); // bit 31 = DTC Reset FPGA
	
	// usleep(500000 /*500ms*/); 
	sleep(3);
	
	registerWrite(0x9100, 0x00008000); //Turn on CFO Emulation Mode for Serdes Reset
	registerWrite(0x9118, 0xffff00ff); //SERDES resets
	registerWrite(0x9118, 0x00000000); //clear SERDES resets

	// usleep(500000 /*500ms*/); 
	sleep(3);
		
	__FE_COUT__ << "Done with DTC Reset." << __E__;
} //end DTCReset()

//========================================================================
void DTCFrontEndInterface::RunROCFEMacro(__ARGS__)
{
	//	std::string feMacroName = __GET_ARG_IN__("ROC_FEMacroName", std::string);
	//	std::string rocUID = __GET_ARG_IN__("ROC_UID", std::string);
	//
	//	__FE_COUTV__(feMacroName);
	//	__FE_COUTV__(rocUID);

	auto feMacroIt = rocFEMacroMap_.find(feMacroStruct.feMacroName_);
	if(feMacroIt == rocFEMacroMap_.end())
	{
		__FE_SS__ << "Fatal error - ROC FE Macro name '" << feMacroStruct.feMacroName_
		          << "' not found in DTC's map!" << __E__;
		__FE_SS_THROW__;
	}

	const std::string& rocUID         = feMacroIt->second.first;
	const std::string& rocFEMacroName = feMacroIt->second.second;

	__FE_COUTV__(rocUID);
	__FE_COUTV__(rocFEMacroName);

	auto rocIt = rocs_.find(rocUID);
	if(rocIt == rocs_.end())
	{
		__FE_SS__ << "Fatal error - ROC name '" << rocUID << "' not found in DTC's map!"
		          << __E__;
		__FE_SS_THROW__;
	}

	rocIt->second->runSelfFrontEndMacro(rocFEMacroName, argsIn, argsOut);

}  // end RunROCFEMacro()

DEFINE_OTS_INTERFACE(DTCFrontEndInterface)
