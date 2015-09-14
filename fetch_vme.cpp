#include "VMEReader.h"
#include "FileConstants.h"

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <signal.h>

#define NUM_TRIG_BEFORE_FILE_CHANGE 1000
#define PATH "/home/ppstb/timing_data/"

using namespace std;

VMEReader* vme;
int gEnd = 0;

void CtrlC(int aSig) {
  if (gEnd==0) { cerr << endl << "[C-c] Trying a clean exit!" << endl; vme->Abort(); }
  else if (gEnd>=5) { cerr << endl << "[C-c > 5 times] ... Forcing exit!" << endl; exit(0); }
  gEnd++;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, CtrlC);
  
  // Where to put the logs
  ofstream err_log("daq.err", ios::binary);
  //const Logger lr(err_log, cerr);
 
  string xml_config;
  if (argc<2) {
    cout << "No configuration file provided! using default config/config.xml" << endl;
    xml_config = "config/config.xml";
  }
  else xml_config = argv[1];

  const unsigned int num_tdc = 1;

  ofstream out_file[num_tdc];
  unsigned int num_events[num_tdc];  

  VME::TDCEventCollection ec;

  VME::AcquisitionMode acq_mode = VME::TRIG_MATCH;
  VME::DetectionMode det_mode = VME::TRAILEAD;
  
  file_header_t fh;
  fh.magic = 0x30535050; // PPS0 in ASCII
  fh.run_id = 0;
  fh.spill_id = 0;
  fh.acq_mode = acq_mode;
  fh.det_mode = det_mode;
  
  time_t t_beg;
  for (unsigned int i=0; i<num_tdc; i++) num_events[i] = 0;
  unsigned long num_triggers = 0, num_all_triggers = 0, num_files = 0;

  try {
    bool with_socket = true;

    vme = new VMEReader("/dev/a2818_0", VME::CAEN_V2718, with_socket);

    // Declare a new run to the online database
    vme->NewRun();

    NIM::HVModuleN470* hv;
    try {
      vme->AddHVModule(0x500000, 0xc);
    } catch (Exception& e) { if (vme->UseSocket()) vme->Send(e); }
    try {
      hv = vme->GetHVModule();
      cout << "module id=" << hv->GetModuleId() << endl;
      hv->ReadMonitoringValues();
      try {
        vme->LogHVValues(0, hv->ReadChannelValues(0));
        vme->LogHVValues(3, hv->ReadChannelValues(3));
      } catch (Exception& e) {;}
    } catch (Exception& e) { if (vme->UseSocket()) vme->Send(e); e.Dump(); }
    /*hv->SetChannelV0(0, 320);
    hv->SetChannelI0(0, 0);
    hv->ReadChannelValues(0);*/

    // Initialize the configuration one single time
    try { vme->ReadXML(xml_config); } catch (Exception& e) {
      if (vme->UseSocket()) vme->Send(e);
    }
 
    fh.run_id = vme->GetRunNumber();
  
    static const unsigned int num_tdc = vme->GetNumTDC();
    
    VME::FPGAUnitCollection fpgas = vme->GetFPGAUnitCollection(); VME::FPGAUnitV1495* fpga;
    for (VME::FPGAUnitCollection::iterator afpga=fpgas.begin(); afpga!=fpgas.end(); afpga++) {
      if (afpga->second->IsTDCControlFanout()) { fpga = afpga->second; break; }
    }
    const bool use_fpga = (fpga!=0);
    if (!use_fpga) {
      std::ostringstream os;
      os << "Trying to launch the acquisition with a configuration\n\t"
         << "in which no FPGA board provide control lines fanout to HPTDCs";
      throw Exception(__PRETTY_FUNCTION__, os.str(), Fatal);
      exit(0);
    }
    fstream out_file[num_tdc];
    string acqmode[num_tdc], detmode[num_tdc];
    int num_triggers_in_files;

    t_beg = time(0);

    // Initial dump of the acquisition parameters before writing the files
    cerr << endl << "*** Ready for acquisition! ***" << endl;
    VME::TDCCollection tdcs = vme->GetTDCCollection(); unsigned int i = 0;
    for (VME::TDCCollection::iterator atdc=tdcs.begin(); atdc!=tdcs.end(); atdc++, i++) {
      switch (atdc->second->GetAcquisitionMode()) {
        case VME::CONT_STORAGE: acqmode[i] = "Continuous storage"; break;
        case VME::TRIG_MATCH: acqmode[i] = "Trigger matching"; break;
        default:
          acqmode[i] = "[Invalid mode]";
          throw Exception(__PRETTY_FUNCTION__, "Invalid acquisition mode!", Fatal);
      }
      switch (atdc->second->GetDetectionMode()) {
        case VME::PAIR: detmode[i] = "Pair measurement"; break;
        case VME::OLEADING: detmode[i] = "Leading edge only"; break;
        case VME::OTRAILING: detmode[i] = "Trailing edge only"; break;
        case VME::TRAILEAD: detmode[i] = "Leading and trailing edges"; break;
      }
    }
    cerr << "Acquisition mode: ";
    for (unsigned int i=0; i<num_tdc; i++) { if (i>0) cerr << " / "; cerr << acqmode[i]; }
    cerr << endl 
         << "Detection mode: ";
    for (unsigned int i=0; i<num_tdc; i++) { if (i>0) cerr << " / "; cerr << detmode[i]; }
    cerr << endl 
         << "Local time: " << asctime(localtime(&t_beg));
    
    if (use_fpga) {
      fpga->StartScaler();
    }

    // Change outputs file once a minimal amount of triggers is hit
    while (true) {
      // First all registers are updated
      i = 0; time_t start = time(0);
      num_triggers_in_files = 0;

      // Declare a new burst to the online DB
      vme->NewBurst();
      fh.spill_id = vme->GetBurstNumber();

      // TDC output files configuration
      for (VME::TDCCollection::iterator atdc=tdcs.begin(); atdc!=tdcs.end(); atdc++, i++) {
        VME::TDCV1x90* tdc = atdc->second;
        
        fh.acq_mode = tdc->GetAcquisitionMode();
        fh.det_mode = tdc->GetDetectionMode();

        ostringstream filename;
        filename << PATH << "/events"
                 << "_" << fh.run_id
                 << "_" << fh.spill_id
                 << "_" << start
                 << "_board" << i
                 //<< "_" GenerateString(4)
                 << ".dat";
        vme->SetOutputFile(atdc->first, filename.str());
        out_file[i].open(vme->GetOutputFile(atdc->first).c_str(), fstream::out | ios::binary);
        if (!out_file[i].is_open()) {
          throw Exception(__PRETTY_FUNCTION__, "Error opening file", Fatal);
        }
        out_file[i].write((char*)&fh, sizeof(file_header_t));
        
      }
      
      // Pulse to set a common starting time for both TDC boards
      if (use_fpga) {
        fpga->PulseTDCBits(VME::FPGAUnitV1495::kReset|VME::FPGAUnitV1495::kClear); // send a RST+CLR signal from FPGA to TDCs
      }
      
      // Data readout from the two TDC boards
      unsigned long tm = 0;
      unsigned long nt = 0;
      uint32_t word;
      while (true) {
        if (use_fpga and (vme->GetGlobalAcquisitionMode()==VMEReader::TriggerStart)) {
          if ((nt=fpga->GetScalerValue())!=num_triggers) { 
            for (unsigned int i=0; i<tdcs.size(); i++) {
              if (out_file[i].is_open()) {
                word = VME::TDCEvent(VME::TDCEvent::Trigger).GetWord();
                out_file[i].write((char*)&word, sizeof(uint32_t));
              }
            }
            num_triggers = nt;
          }
          /*else if (cnt_without_new_trigger>1000) { break; cnt_without_new_trigger = 0; }
          cnt_without_new_trigger++;*/ //FIXME new feature to be tested before integration
        }
        unsigned int i = 0; tm += 1;
        for (VME::TDCCollection::iterator atdc=tdcs.begin(); atdc!=tdcs.end(); atdc++, i++) {
          ec = atdc->second->FetchEvents();
          if (ec.size()==0) continue; // no events were fetched
          for (VME::TDCEventCollection::const_iterator e=ec.begin(); e!=ec.end(); e++) {
            word = e->GetWord();
            out_file[i].write((char*)&word, sizeof(uint32_t));
            /*cout << dec << "board" << i << ": " << hex << e->GetType() << "\t";
            if (e->GetType()==VME::TDCEvent::TDCMeasurement) cout << e->GetChannelId();
            cout << endl;*/
            //e->Dump();
            //if (e->GetType()==VME::TDCEvent::TDCMeasurement) cout << "----> (board " << dec << i << " with address " << hex << atdc->first << dec << ") new event on channel " << e->GetChannelId() << endl;
          }
          num_events[i] += ec.size();
        }
        if (use_fpga and tm>5000) { // probe the scaler value every N data readouts
          if (vme->GetGlobalAcquisitionMode()!=VMEReader::TriggerStart) num_triggers = fpga->GetScalerValue();
          try {
            vme->BroadcastHVStatus(0, hv->ReadChannelValues(0));
            vme->BroadcastHVStatus(3, hv->ReadChannelValues(3));
          } catch (Exception& e) {;}
          num_triggers_in_files = num_triggers-num_all_triggers;
            cerr << "--> " << num_triggers << " triggers acquired in this run so far" << endl;
          if (num_triggers_in_files>0 and num_triggers_in_files>=NUM_TRIG_BEFORE_FILE_CHANGE) {
            num_all_triggers = num_triggers;
            break; // break the infinite loop to write and close the current file
          }
          tm = 0;
        }
      }
      num_files += 1;
      cerr << "---> " << num_triggers_in_files << " triggers written in current TDC output files" << endl;
      unsigned int i = 0;
      for (VME::TDCCollection::const_iterator atdc=tdcs.begin(); atdc!=tdcs.end(); atdc++, i++) {
        if (out_file[i].is_open()) out_file[i].close();
        cout << "Sent output from TDC 0x" << hex << atdc->first << dec << " in spill id " << fh.spill_id << endl;
        vme->SendOutputFile(atdc->first); usleep(1000);
      }
      vme->BroadcastNewBurst(fh.spill_id); usleep(1000);
      vme->BroadcastTriggerRate(fh.spill_id, num_triggers);
    }
  } catch (Exception& e) {
    // If any TDC::FetchEvent method throws an "acquisition stop" message
    if (e.ErrorNumber()==TDC_ACQ_STOP) {
      unsigned int i = 0;
      try {
        VME::TDCCollection tdcs = vme->GetTDCCollection();
        for (VME::TDCCollection::const_iterator atdc=tdcs.begin(); atdc!=tdcs.end(); atdc++, i++) {
          if (out_file[i].is_open()) out_file[i].close();
          vme->SendOutputFile(atdc->first); usleep(1000);
        }
  
        time_t t_end = time(0);
        double nsec_tot = difftime(t_end, t_beg), nsec = fmod(nsec_tot,60), nmin = (nsec_tot-nsec)/60.;
        cerr << endl << "*** Acquisition stopped! ***" << endl
             << "Local time: " << asctime(localtime(&t_end))
             << "Total acquisition time: " << difftime(t_end, t_beg) << " seconds"
             << " (" << nmin << " min " << nsec << " sec)"
             << endl;

        cerr << endl << "Acquired ";
        for (unsigned int i=0; i<num_tdc; i++) { if (i>0) cerr << " / "; cerr << num_events[i]; }
        cerr << " words in " << num_files << " files for " << num_triggers << " triggers in this run" << endl;
    
        ostringstream os;
        os << "Acquired ";
        for (unsigned int i=0; i<num_tdc; i++) { if (i>0) os << " / "; os << num_events[i]; }
        os << " words in " << num_files << " files for " << num_triggers << " triggers in this run" << endl
           << "Total acquisition time: " << difftime(t_end, t_beg) << " seconds"
           << " (" << nmin << " min " << nsec << " sec)";
        if (vme->UseSocket()) vme->Send(Exception(__PRETTY_FUNCTION__, os.str(), Info));
      
        delete vme;
      } catch (Exception& e) { e.Dump(); }
      return 0;
    }
    e.Dump();
    if (vme->UseSocket()) vme->Send(e);
    return -1;
  }
    
  return 0;
}
