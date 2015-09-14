#include "VME_FPGAUnitV1495.h"

namespace VME
{
  FPGAUnitV1495::FPGAUnitV1495(int32_t bhandle, uint32_t baseaddr) :
    GenericBoard<FPGAUnitV1495Register,cvA32_U_DATA>(bhandle, baseaddr)
  {
    try {
      CheckBoardVersion();
      ClearOutputPulser();
      StopScaler();
    } catch (Exception& e) { e.Dump(); throw e; }

    unsigned short cfwrev = GetCAENFirmwareRevision();
    unsigned int hwrev = GetHardwareRevision();
    std::ostringstream os;
    os << "New FPGA unit added at base address 0x" << std::hex << fBaseAddr << "\n\t"
       << "Serial number: " << std::dec << GetSerialNumber() << "\n\t"
       << "Hardware revision: " << std::dec
                                << ((hwrev>>24)&0xff) << "."
                                << ((hwrev>>16)&0xff) << "."
                                << ((hwrev>>8) &0xff) << "."
                                << ( hwrev     &0xff) << "\n\t"
       << "CAEN Firmware revision: " << std::dec << ((cfwrev>>8)&0xff) << "." << (cfwrev&0xff) << "\n\t"
       << "Geo address: 0x" << std::hex << GetGeoAddress();
    PrintInfo(os.str());
    GetControl();
    DumpFWInformation();
  }

  FPGAUnitV1495::~FPGAUnitV1495()
  { ; }
 
  void
  FPGAUnitV1495::DumpFWInformation() const
  {
    unsigned short ufwrev = GetUserFirmwareRevision();
    FPGAUnitV1495Control control = GetControl();

    std::ostringstream os;
    os << "User Firmware information" << "\n\t"
       << "FW revision: " << std::dec << ((ufwrev>>8)&0xff) << "." << (ufwrev&0xff) << "\n\t"
       << "Clock source:   ";
    switch (control.GetClockSource()) {
      case FPGAUnitV1495Control::ExternalClock: os << "external"; break;
      case FPGAUnitV1495Control::InternalClock:
        os << "internal (period: " << (std::max(GetInternalClockPeriod(),(uint32_t)1)*25) << " ns)"; break;
      default: os << "invalid (" << control.GetClockSource() << ")"; break;
    }
    os << "\n\t"
       << "Trigger source: ";
    switch (control.GetTriggerSource()) {
      case FPGAUnitV1495Control::ExternalTrigger: os << "external"; break;
      case FPGAUnitV1495Control::InternalTrigger:
        os << "internal (period: " << (std::max(GetInternalTriggerPeriod(),(uint32_t)1)*25/1e6) << " ms)"; break;
      default: os << "invalid (" << control.GetTriggerSource() << ")"; break;
    }
    PrintInfo(os.str());
  }

  unsigned short
  FPGAUnitV1495::GetCAENFirmwareRevision() const
  {
    uint16_t word = 0x0;
    try {
      ReadRegister(kV1495FWRevision, &word);
      return static_cast<unsigned short>(word&0xffff);
    } catch (Exception& e) { e.Dump(); }
    return word;
  }

  unsigned short
  FPGAUnitV1495::GetUserFirmwareRevision() const
  {
    uint16_t word = 0x0;
    try {
      ReadRegister(kV1495UserFWRevision, &word);
      return static_cast<unsigned short>(word&0xffff);
    } catch (Exception& e) { e.Dump(); }
    return word;
  }

  unsigned int
  FPGAUnitV1495::GetHardwareRevision() const
  {
    uint16_t word = 0x0;
    uint32_t hwrev = 0x0;
    try {
      ReadRegister(kV1495HWRevision0, &word); hwrev  =  (word&0xff);
      ReadRegister(kV1495HWRevision1, &word); hwrev |= ((word&0xff)<<8);
      ReadRegister(kV1495HWRevision2, &word); hwrev |= ((word&0xff)<<16);
      ReadRegister(kV1495HWRevision3, &word); hwrev |= ((word&0xff)<<24);
      return hwrev;
    } catch (Exception& e) { e.Dump(); }
    return hwrev;
  }

  unsigned short
  FPGAUnitV1495::GetSerialNumber() const
  {
    uint16_t word = 0x0;
    uint16_t sernum;
    try {
      ReadRegister(kV1495SerNum1, &word); sernum  =  (word&0xff);
      ReadRegister(kV1495SerNum0, &word); sernum |= ((word&0xff)<<8);
      return sernum;
    } catch (Exception& e) { e.Dump(); }
    return 0;
  }

  unsigned short
  FPGAUnitV1495::GetGeoAddress() const
  {
    uint16_t word = 0x0;
    try {
      ReadRegister(kV1495GeoAddress, &word);
      return (word);
    } catch (Exception& e) { e.Dump(); }
    return 0;
  }

  void
  FPGAUnitV1495::CheckBoardVersion() const
  {
    uint16_t word = 0x0;
    uint16_t oui0 = 0x0, oui1 = 0x0, oui2 = 0x0;
    uint16_t board0 = 0x0, board1 = 0x0, board2 = 0x0;

    // OUI
    try {
      ReadRegister(kV1495OUI0, &word); oui0 = word&0xff;
      ReadRegister(kV1495OUI1, &word); oui1 = word&0xff;
      ReadRegister(kV1495OUI2, &word); oui2 = word&0xff;
    } catch (Exception& e) { throw e; }
    if (oui0!=0xe6 or oui1!=0x40 or oui2!=0x00) {
      std::ostringstream os;
      os << "Wrong OUI version: " << std::hex
         << oui0 << "/" << oui1 << "/" << oui2;
      throw Exception(__PRETTY_FUNCTION__, os.str(), JustWarning);
    }

    // Board version
    try {
      ReadRegister(kV1495Board0, &word); board0 = word&0xff;
      ReadRegister(kV1495Board1, &word); board1 = word&0xff;
      ReadRegister(kV1495Board2, &word); board2 = word&0xff;
    } catch (Exception& e) { throw e; }
    if (board0!=0xd7 or board1!=0x05 or board2!=0x00) {
      std::ostringstream os;
      os << "Wrong board version: " << std::hex
         << board0 << "/" << board1 << "/" << board2;
      throw Exception(__PRETTY_FUNCTION__, os.str(), JustWarning);
    }
  }

  unsigned short
  FPGAUnitV1495::GetTDCBits() const
  {
    uint32_t word = 0x0;
    try {
      ReadRegister(kV1495TDCBoardInterface, &word);
      return static_cast<unsigned short>(word&0x7);
    } catch (Exception& e) { e.Dump(); }
    return 0;
  }

  void
  FPGAUnitV1495::SetTDCBits(unsigned short bits) const
  {
    uint32_t word = (bits&0x7);
    try {
      if (bits==GetTDCBits()) return;
      WriteRegister(kV1495TDCBoardInterface, word);
    } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set TDC bits", JustWarning);
    }
  }

  void
  FPGAUnitV1495::PulseTDCBits(unsigned short bits, unsigned int time_us) const
  {
    // FIXME need to check what is the previous bits status before any pulse -> exception?
    try {
      SetTDCBits(bits);
      usleep(time_us); // we let it high for a given time (in us)
      SetTDCBits(0x0);
    } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to send a pulse to TDC bits", JustWarning);
    }
  }

  FPGAUnitV1495Control
  FPGAUnitV1495::GetControl() const
  {
    uint32_t word = 0x0;
    sleep(1);
    try { ReadRegister(kV1495Control, &word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to retrieve the control word from FW", JustWarning);
    }
    return FPGAUnitV1495Control(word);
  }

  void
  FPGAUnitV1495::SetControl(const FPGAUnitV1495Control& control) const
  {
    sleep(1);
    try { WriteRegister(kV1495Control, control.GetWord()); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set the control word to FW", JustWarning);
    }
  }

  void
  FPGAUnitV1495::ResetFPGA() const
  {
    uint32_t word = 0x1;
    try { WriteRegister(kV1495UserFPGAConfig, word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to reset the FPGA configuration", JustWarning);
    }
  }

  void
  FPGAUnitV1495::SetInternalClockPeriod(uint32_t period) const
  {
    //if (period<=0 or (period>1 and period%2!=0)) {
    if (period<=0) {
      std::ostringstream os; os << "Trying to set an invalid clock period (" << period << ")";
      throw Exception(__PRETTY_FUNCTION__, os.str(), JustWarning);
    }
    try { WriteRegister(kV1495ClockSettings, period); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set internal clock's period", JustWarning);
    }
  }

  uint32_t
  FPGAUnitV1495::GetInternalClockPeriod() const
  {
    uint32_t word;
    try { ReadRegister(kV1495ClockSettings, &word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to retrieve internal clock's period", JustWarning);
      return 0;
    }
    return word;
  }

  void
  FPGAUnitV1495::SetInternalTriggerPeriod(uint32_t period) const
  {
    if (period<=0) {
      std::ostringstream os; os << "Trying to set an invalid trigger period (" << period << ")";
      throw Exception(__PRETTY_FUNCTION__, os.str(), JustWarning);
    }
    try { WriteRegister(kV1495TriggerSettings, period); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set internal trigger's period", JustWarning);
    }
  }

  uint32_t
  FPGAUnitV1495::GetInternalTriggerPeriod() const
  {
    uint32_t word;
    try { ReadRegister(kV1495TriggerSettings, &word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to retrieve internal trigger's period", JustWarning);
      return 0;
    }
    return word;
  }

  uint32_t
  FPGAUnitV1495::GetOutputPulser() const
  {
    uint32_t word;
    try { ReadRegister(kV1495OutputSettings, &word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to retrieve output pulser's word", JustWarning);
      return 0;
    }
    return word;
  }

  void
  FPGAUnitV1495::ClearOutputPulser() const
  {
    uint32_t word = 0x0;
    try { WriteRegister(kV1495OutputSettings, word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to clear output pulser's word", JustWarning);
    }
  }

  void
  FPGAUnitV1495::SetOutputPulser(unsigned short id, bool enable) const
  {
    uint32_t word = GetOutputPulser();
    if (word>> id) word -= (1<< id);
    if (enable)    word += (1<< id);
    //std::cout << word << std::endl;
    try { WriteRegister(kV1495OutputSettings, word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set output pulser's word", JustWarning);
    }
  }
  
  uint32_t
  FPGAUnitV1495::GetOutputDelay() const
  {
    uint32_t word = 0x0;
    try { ReadRegister(kV1495DelaySettings, &word); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to retrieve output delay word", JustWarning);
    }
    return word;
  }

  void
  FPGAUnitV1495::SetOutputDelay(uint32_t delay) const
  {
    try { WriteRegister(kV1495DelaySettings, delay); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set output delay word", JustWarning);
    }
  }

  void
  FPGAUnitV1495::SetOutputPulserPOI(uint32_t poi) const
  {
    try { WriteRegister(kV1495OutputSettings, poi); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set output pulser's POI word", JustWarning);
    }
  }

  void
  FPGAUnitV1495::StartScaler()
  {
    try {
      FPGAUnitV1495Control c = GetControl();
      // first we stop and reset the counter
      c.SetScalerStatus(false);
      c.SetScalerReset(true);
      SetControl(c);
      usleep(10000);
      // then we start the counter (removing the "reset" bit)
      c.SetScalerStatus(true);
      c.SetScalerReset(false);
      SetControl(c);
      usleep(10000);
    } catch (Exception& e) { e.Dump(); }
  }

  void
  FPGAUnitV1495::StopScaler()
  {
    try {
      FPGAUnitV1495Control c = GetControl();
      c.SetScalerStatus(false);
      SetControl(c);
    } catch (Exception& e) { e.Dump(); }
  }

  uint32_t
  FPGAUnitV1495::GetScalerValue() const
  {
    uint32_t value = 0;
    try { ReadRegister(kV1495ScalerCounter, &value); } catch (Exception& e) {
      e.Dump();
    }
    return value;
  }

 uint32_t
  FPGAUnitV1495::GetThresholdVoltage(uint32_t tdc_number) const
  {
    uint32_t voltage = 0x0;
    sleep(1);
    try {if (tdc_number < 2)
          ReadRegister(kV1495ThresholdVoltage0, &voltage);
      else
          ReadRegister(kV1495ThresholdVoltage1, &voltage); } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to retrieve the threshold voltage from FW", JustWarning);
    }
    return voltage;
  }

  void
  FPGAUnitV1495::SetThresholdVoltage(uint32_t voltage, uint32_t tdc_number) const
  {
    voltage = voltage % 1024; 	// DAC threshold voltage is difference between two 12-bit numbers
    uint32_t oldvoltage = 0x0;
    uint32_t digitalValue = 0x0;
    sleep(1);
    try { 
      if (tdc_number < 2)
         ReadRegister(kV1495ThresholdVoltage0, &oldvoltage);
      else
         ReadRegister(kV1495ThresholdVoltage1, &oldvoltage);
      if (tdc_number == 0){
	 WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0xFFFF0000) + OneVolt);
         sleep(1);
         digitalValue = (uint32_t) (1.758683*voltage-15.74276+0.5);
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0xFFFF0000) + cH1 + OneVolt + digitalValue);
         sleep(1);
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0xFFFF0000) + cH2 + OneVolt);
         sleep(1);
         digitalValue = (uint32_t) (1.753127*voltage-15.45482+0.5);
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0xFFFF0000) + cH3 + OneVolt + digitalValue);
      }
      if (tdc_number == 1){
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0x0000FFFF) + 0x00010000 * OneVolt);
         sleep(1);
         digitalValue = (uint32_t) (1.758683*voltage-15.74276+0.5);
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0x0000FFFF) + 0x00010000 * (cH1 + OneVolt + digitalValue));
         sleep(1);
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0x0000FFFF) + 0x00010000 * (cH2 + OneVolt));
         sleep(1);
         digitalValue = (uint32_t) (1.753127*voltage-15.45482+0.5);
         WriteRegister(kV1495ThresholdVoltage0, (oldvoltage & 0x0000FFFF) + 0x00010000 * (cH3 + OneVolt + digitalValue));
      }
      if (tdc_number == 2){
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0xFFFF0000) + OneVolt);
         sleep(1);
         digitalValue = (uint32_t) (1.70261*voltage-30.023+0.5);
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0xFFFF0000) + cH1 + OneVolt + digitalValue);
         sleep(1);
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0xFFFF0000) + cH2 + OneVolt);
         sleep(1);
         digitalValue = (uint32_t) (1.712739*voltage-3.96861+0.5);
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0xFFFF0000) + cH3 + OneVolt + digitalValue);
      }
      if (tdc_number == 3){
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0x0000FFFF) + 0x00010000 * OneVolt);
         sleep(1);
         digitalValue = (uint32_t) (1.703834*voltage-0.54032+0.5);
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0x0000FFFF) + 0x00010000 * (cH1 + OneVolt + digitalValue));
         sleep(1);
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0x0000FFFF) + 0x00010000 * (cH2 + OneVolt));
         sleep(1);
         digitalValue = (uint32_t) (1.702613*voltage+20.28103+0.5);
         WriteRegister(kV1495ThresholdVoltage1, (oldvoltage & 0x0000FFFF) + 0x00010000 * (cH3 + OneVolt + digitalValue));
      }
    } catch (Exception& e) {
      e.Dump();
      throw Exception(__PRETTY_FUNCTION__, "Failed to set the threshold voltage to FW", JustWarning);
    }
  }

}
