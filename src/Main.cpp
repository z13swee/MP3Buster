#include <iostream>
#include <chrono> //milliseconds
#include <thread> //sleep_for
#include <getopt.h>
#include <filesystem> // C++17
#include <algorithm>
#include <map>

#include <alsa/asoundlib.h> /* dnf install alsa-lib-devel */ /* apt install libasound2-dev */
#include "mpeg/Mp3.hpp"
#include "logger.hpp"


// Set log level
// unsigned GlobalLogLevel = LOG_VERBOSE;
unsigned GlobalLogLevel = LOG_INFO;
// unsigned GlobalLogLevel = LOG_WARNING;
// unsigned GlobalLogLevel = LOG_ERROR;
// unsigned GlobalLogLevel = LOG_SILENT;


void version() {
  std::cout << "MP3Crawler 1.0" << std::endl;
  std::cout << "Copyright (C) 2022 Sebastian Ekman." << std::endl;
}

void usage()
{
    std::cout << "\nUsage:\tMP3Crawler [OPTIONS]...  FILE|PATH" << std::endl;
    std::cout << "Batchmode will only output one line per file with information" << std::endl;
    std::cout << "  -n,\t\t--nobatch\t\t\tStops batchmode for multiple files and will show full information" << std::endl;
    std::cout << "  -c <path>,\t\t--config <path>\t\tpath to config file describeing action to be taken" << std::endl;
    std::cout << "  -e <path>,\t--erroroutput <path>\tPuts full path of the files giving errors into a file" << std::endl;
    std::cout << "  -l,\t\t--log <1-6>\t\tdetermines the log level (see log levels below) thats get printed" << std::endl;
    std::cout << "  -r,\t\t--recursive\t\tRecursivly going trought given path" << std::endl;

    std::cout << "  -p,\t\t--playback\t\tplays the audio from the mp3 file" << std::endl;
    std::cout << "  -d,\t\t--dontstop\t\tDont stop at first bad frame, cuntinue analyzeing. Only for batchmode" << std::endl;
    std::cout << "    \t\t--help\t\t\tprint this help screen" << std::endl;
    std::cout << "    \t\t--version\t\tprint version information\n" << std::endl;
    std::cout << "Loglevels:" << std::endl;
    std::cout << "\t1: Error" << std::endl;
    std::cout << "\t2: Errors and Warnings" << std::endl;
    std::cout << "\t3: Errors and Warnings and information (default)" << std::endl;
    std::cout << "\t4: Errors and Warnings and Information and Verbos" << std::endl;

    // TODO: Info about config file ??

}

/*
  (new) WORKFLOW: !=TODO *=DONE
    * Given one file, the program gives a summary (either normal summary or verbose summary).
    ! Given two files, the program gives out a comparision between the two
    ! Given more then two files or folders, the program gives ut bulk-mode information

  TODO:
   MAJOR:
    * history, a temporary locale file thats keeps the hashes of the known good files. so when it runs again, it wont check thoes
      and a option for ingnoring history and re-scan
    * Profiler? to make it faster..

  MINOR:
    * Tidy up File.hpp/cpp , implement and test write functions etc.
    * Tidy up argument/config hanteraren
    * Process indekation? 24/144 files counter or something?
    * Better visualized seperation between files, se running multiple files with -b
    * Add as options:
        + path to configuration file that describe how to manage the mp3 files, rename,
          rephrase, delete/put in corantine etc.
        + Options to output a file with list of only bad mp3s?

  NOT EVEN STARTED:
    * File & Tag manager:
      + Name convention for known bands. ie. I say that all ACDC songs shouled be spelled:
        'AC/DC'. Then the manager will 'convert' all verationts of ACDC to that spelling..
      + AudioRanger will complete missing information with data obtained from high quality
        online sources like the music databases MusicBrainz and AcoustID.
      + Add high quality album artwork
      + Use different name patterns for single artist albums, compilation albums and single tracks

  TEST:
    Går det att fixa dessa miss ljud genom att ta 'sy ihop' dom fungerande frames en?
    Alltså flytta nästa valid frame till den plats som den 'ska vara'

  BUGG:
    + Vid Batchmode output så blir det något fel med std::setw eller std::left eller nått, för
     när det kommer en fil med ÅÄÖ i sig så blir det inte lika "brätt" till ':OK' outputen
     (se: https://stackoverflow.com/questions/22040052/stdsetw-considering-special-characters-as-two-characters)

    + ~/Mp3/Game\ OST/MP3/Sanitarium/01\ -\ track\ 01.mp3 gibes sample rate error and does not preocess even though audacious plays it
    + Trunctace error is given in single file mode, but shows OK in bulk mode (jamp3-testfiles/mp3/vbr/abr064.mp3)
  
  Fundering:

   Det verkar som att EEEK ljudet skapas av när mpeg headern inte är i sync, alltså på det stället det borde.
   Dock 2 fel syncade frames i närheten av varandra ger bara ett 'hick' i ljudet.
   Se Kalle Bah - 03 - You are my angel.mp3.
   Fast kan ge ett konstigt ljud också..!? se Beastie Boys - Fight for your right to party

   men fyra miss sync i närheten av varandra ger EEEK!, se Rem - Man on the moon.mp3
   Det finns 2 dåliga frames i Meli C - Nothen Star, när jag spelar så blir det ingen eek,
   men med ffplay så blir det eek ??

   1 Bad Frame , ger konstigt ljud i bland..
   Ella Fitz- Cheek to Cheek.mp3 (ffplay) <- inget konstigt ljud
   Eric Carmen - All By Myself.mp3 <- konstigt ljud

   Kolla på PCM på framen före, under och efter bad frame på båda dessa filer,
   så kanske man kan se en skillnad som kan berätta ifall det orsakar en EEK ?

   Xing visar antal frames som ex. 1031, men jag räknar till 1032. Är det för att xing
   uptar en tom mpeg frame och räknar inte med den? (testat på jamp3-test/mp3/vbr/abr032.mp3)

   Nu uppskattas längden på ljudet utifrån första mpeg frame (VBRI,Xing eller CBR)
   Och detta är bra vid "fast mode", men kanske korrekt läsa längden av ljudet när
   hela filen läses? räkna ihop alla frame size * bitrate??

*/
// #define AV_NOPTS_VALUE          ((int64_t)UINT64_C(0x8000000000000000))
// #define AV_TIME_BASE            1000000
//
// if (ic->duration != AV_NOPTS_VALUE) {
//             int64_t hours, mins, secs, us;
//             int64_t duration = ic->duration + (ic->duration <= INT64_MAX - 5000 ? 5000 : 0);
//             secs  = duration / AV_TIME_BASE;
//             us    = duration % AV_TIME_BASE;
//             mins  = secs / 60;
//             secs %= 60;
//             hours = mins / 60;
//             mins %= 60;
//             av_log(NULL, AV_LOG_INFO, "%02"PRId64":%02"PRId64":%02"PRId64".%02"PRId64"", hours, mins, secs,
//                    (100 * us) / AV_TIME_BASE);
//         }



void AddToQue(std::filesystem::path path, myConfig& cfg, std::vector<std::filesystem::path>& queue) {
  // TODO: Check content if this realy is a mp3 file. Now we only go by extension
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(),ext.begin(), ::toupper);

  if(ext == ".MP3"){
    std::cout << "Adding " << path << " to que" << std::endl;
    queue.push_back(path);

    if(path.filename().string().length() > cfg.longestWidthEntry)
      cfg.longestWidthEntry = path.filename().string().length();
  }
}

int HandleArgumentsRequest(int argc, char *argv[], myConfig& cfg, std::vector<std::filesystem::path>& queue ) {


  // argc (ARGument Count) is int and stores number of command-line arguments passed by
  // the user including the name of the program. So if we pass a value to a program,
  // value of argc would be 2 (one for argument and one for program name)

  // If we got no arguments we exit
  if(argc == 1){
    std::cout << "No arguments" << std::endl;
    return 0;
  }

  // The variable optind is the index of the next element to be processed in argv.
  // The system initializes this value to 1. The caller can reset it to 1 to restart
  // scanning of the same argv, or when scanning a new argument vector.


  // The struct option structure has these fields:

  // const char *name - This field is the name of the option. It is a string.
  // int has_arg      - This field says whether the option takes an argument. It is an integer, and there are three legitimate values:
  //                      no_argument,
  //                      required_argument,
  //                      optional_argument.
  // int *flag        - If flag is a null pointer, then the val is a value which identifies this option. Often these values are chosen to uniquely identify particular long options.
  //                    If flag is not a null pointer, it should be the address of an int variable which is the flag for this option.
  //
  // int val          The value in val is the value to store in the flag to indicate that the option was seen.

  static struct option long_options[] =
    {
      {"help",    no_argument, 0, 'h'}, // Print out usage
      {"version", no_argument, 0, 'V'}, // Print out version
      {"nobatch", no_argument, 0, 'n'},
      {"erroroutput",   required_argument, 0, 'e'},
      {"config",   required_argument, 0, 'c'},
      {"log",   required_argument, 0, 'l'},
      {"recursive",   no_argument, 0, 'r'},
      {"playback",   no_argument, 0, 'p'},
      {"errorstop",   no_argument, 0, 'e'},
      {0, 0, 0, 0}
    };

  // Short options (note: No short options for version, help)
  const char* short_options="ncelprd";

  int c;
  int option_index = -1;


  // optind - The variable optind is the index of the next element to be processed in argv.
  //          The system initializes this value to 1. The caller can reset it to 1 to restart
  //          scanning of the same argv, or when scanning a new argument vector.
  // argc - arguments count
  // argv - argument array
  while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1)
  {

    switch (c)
    {
        case 'h': { usage(); break; }
        case 'V': { version(); break; }
        case 'n': { cfg.stopbatchmode = true; break; }
        case 'c': { std::cout << "config option!" << std::endl; break; }
        case 'e': {

          // So if we useing long option, the value is in optarg
          // else we need to fetch next char
          if(option_index >= 0)
            cfg.erroroutputpath = optarg;
          else
            cfg.erroroutputpath = argv[optind];
            optind++; // ..and move index forward

          if(cfg.erroroutputpath != "") {
            cfg.erroroutput = true;
          }
          break;
        }

        case 'l': {
          int value;

          if(option_index >= 0) {
            value = strtol(optarg, NULL, 10);
          } else {
            value = strtol(argv[optind], NULL, 10);
            optind++; // ..and move index forward
          }

          if(value >= 0 && value <= 6) {
            cfg.loglevel = value;
          } else {
            debug(LOG_WARNING) << "Invalid log level given, going with default log level " << cfg.loglevel << std::endl;
          }
          break;
        }

        case 'p': { cfg.playback = true; break; }
        case 'r': { cfg.recursive = true; break; }
        case 'd': { cfg.stoponerror = false; break; }

        /* missing option argument */
        case ':': {
          debug(LOG_WARNING) << argv[0] << ": option `-" << optopt << "' requires an argument, going with default value" << std::endl;
          break;
        }

        /* invalid option */
        case '?':
        default:
            // std::cerr << argv[0] << ": option -" << (char)optopt << " is invalid and is ignored" << std::endl;
            return 0;
            break;


    }
  }

  // Set loglevel
  GlobalLogLevel = cfg.loglevel;

  std::cout << "argc: " << argc << std::endl;
  std::cout << "optind: " << optind << std::endl;

  if(optind < argc) {
    for(int i = optind; i<argc; i++) {
      std::cout << "Analyzeing argument: " << argv[i] << std::endl;

      std::filesystem::path path(argv[i]);
      std::error_code error_code; // For using the non-throwing overloads of functions below.

      // Check if argument is a directory
      if (std::filesystem::is_directory(path, error_code)) {
        for(auto itEntry = std::filesystem::recursive_directory_iterator(argv[i]);
             itEntry != std::filesystem::recursive_directory_iterator();
             ++itEntry )
        {
          // Note: Recursive is regulated by depth()
          if(!cfg.recursive && itEntry.depth() > 0)
            continue;

          if (itEntry->is_regular_file())
            AddToQue(itEntry->path(), cfg, queue);

        }
      }

      if (error_code) {
          debug(LOG_ERROR) << "Error: " << error_code.message();
          return 0;
      }



      if (std::filesystem::is_regular_file(path, error_code))
        AddToQue(path, cfg, queue);


      if (error_code) {
          debug(LOG_ERROR) << "Error: " << error_code.message();
          return 0;
      }

    }
  }


  // Dont run in batchmode if we only have one path
  if(queue.size() == 1)
    cfg.batchmode = false;

  if(queue.empty())
    debug(LOG_INFO) << "No files found!" << std::endl;

  return 1;
};



int main(int argc, char *argv[])
{

  myConfig cfg;
  std::vector<std::filesystem::path> queue = {};

  // populate config and que according to incoming arguments
  if(HandleArgumentsRequest(argc, argv, cfg, queue)) {
    // Now we should have a config with settings and que with one or more paths


    // If given only one file
    if(queue.size() == 1) {
      MP3 mp3(queue[0], cfg);
      exit(EXIT_SUCCESS);
    }

    // If given two files, then do a compare
    if(queue.size() == 2) {
      std::cout << "COMPARING MODE!" << std::endl;
      
      // in Compare mode, always analys whole files
      cfg.stoponerror = false;

      // Shsss shsss. quiet
      GlobalLogLevel = LOG_SILENT;
      
      MP3 A_mp3(queue[0], cfg);
      MP3 B_mp3(queue[1], cfg);

      // Print compared data (Highlight diffrence)
      // To compare: Duration, 

      exit(EXIT_SUCCESS);
    }

    // If given more then two files ..
    // If option stopbatchmode was given, stop batchmode :>
    if(cfg.stopbatchmode)
      cfg.batchmode = false;

    for(auto p : queue){
      MP3 mp3(p, cfg);


      // Create error output file options is set
      if(cfg.erroroutput) {

        if(mp3.errors){
          // This file has errors!

          // Check if we have an output stream
          if(!cfg.errorourputStream) {
            cfg.errorourputStream = new std::ofstream(cfg.erroroutputpath.c_str());

            // Check for errors ..
            if((cfg.errorourputStream->rdstate() & std::ofstream::failbit) != 0) {
              cfg.erroroutput = false;
              debug(LOG_ERROR) << "Failed creating output file.." << std::endl;
            }
          }

          // Give absolute path to ouput stream
          if(cfg.errorourputStream)
            *cfg.errorourputStream << std::filesystem::absolute(mp3.getPath()).string() << std::endl;

        }
      }

    }

    if(cfg.errorourputStream) {
      if(cfg.errorourputStream->is_open())
        cfg.errorourputStream->close();

      delete cfg.errorourputStream;
    }

  } else {
    usage();
  }

}
