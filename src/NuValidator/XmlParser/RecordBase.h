//_____________________________________________________________________________
/*!

\class    genie::nuvld::RecordBase

\brief

\author   Costas Andreopoulos (Rutherford Lab.)  <costas.andreopoulos \at stfc.ac.uk>

\created  August 2003
*/
//_____________________________________________________________________________

#ifndef _RECORD_BASE_H_
#define _RECORD_BASE_H_

#include <iostream>
#include <string>
#include <vector>
#include <map>

using std::ostream;
using std::string;
using std::vector;
using std::map;

namespace genie {
namespace nuvld {
  
class RecordBase
{
public:

   RecordBase();
   virtual ~RecordBase();

   friend ostream & operator << (ostream & stream, const RecordBase & rec_base);

   virtual void                 PrintStructure(void) const          = 0;
   virtual const vector<string> GetElements(void) const             = 0;
   virtual const vector<string> GetAttributes(string element) const = 0;

   virtual void                 Add(string key, string value)        = 0;
   virtual const string         Get(string key) const                = 0;   

protected:

  map<string, string > _rec;
};

} // nuvld namespace
} // genie namespace

#endif // _RECORD_BASE_H_