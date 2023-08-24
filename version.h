//**************************************************************************************
//  version    changes
//  =======    ======================================
//    1.00     Initial release
//    1.01     > Add counter to show status of string construction.
//               Otherwise, "password manager" takes several minutes to 
//               complete, and the program appears hung while this runs.
//             > Add separate thread to handle search, so messages are
//               still processed during search.  We also want to add an
//               ABORT button to the program, for long searches.
//    1.02     Add field to modify minimum word length
//    1.03     Reverse results list, so largest words are first.
//             This seems more likely to provide most-interesting results.
//    1.04     > Convert to using new classes
//             > Create new string-list class
//    1.05     Switch to generic functions for creating status bar and
//             up/down control
//    1.06     Add exclusion list, after ! at end of word list.
//             Example:
//                 led zeppelin kiss ! zip zen
//    1.07     Add About dialog, do other code modernization
//****************************************************************************
#define VerNum    "V1.07"
