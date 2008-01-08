/*
 * Copyright (c) 2001-2008
 *     DecisionSoft Limited. All rights reserved.
 * Copyright (c) 2004-2008
 *     Oracle. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Id$
 */

#include <iostream>
#include <vector>
#include <map>

#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/XMLUri.hpp>

//XQilla includes
#include <xqilla/xqilla-simple.hpp>
#include <xqilla/ast/LocationInfo.hpp>
#include <xqilla/context/MessageListener.hpp>
#include <xqilla/utils/PrintAST.hpp>
#include <xqilla/events/EventSerializer.hpp>
#include <xqilla/events/NSFixupFilter.hpp>
#include <xqilla/xerces/XercesConfiguration.hpp>
#include <xqilla/fastxdm/FastXDMConfiguration.hpp>

#if defined(XERCES_HAS_CPP_NAMESPACE)
XERCES_CPP_NAMESPACE_USE
#endif

using namespace std;

#define QUERY_BUFFER_SIZE 32 * 1024
#define BASEURI_BUFFER_SIZE 2 * 1024

////////////////////////////
// function declarations  //
////////////////////////////

/** Print usage */
void usage(const char *progname);

class MessageListenerImpl : public MessageListener
{
public:
  virtual void warning(const XMLCh *message, const LocationInfo *location)
  {
    cerr << UTF8(location->getFile()) << ":" << location->getLine() << ":" << location->getColumn()
	 << ": warning: " << UTF8(message) << endl;
  }

  virtual void trace(const XMLCh *label, const Sequence &sequence, const LocationInfo *location, const DynamicContext *context)
  {
    cerr << UTF8(location->getFile()) << ":" << location->getLine() << ":" << location->getColumn()
	 << ": trace: " << UTF8(label) << " ";

    size_t len = sequence.getLength();
    if(len == 1) {
      cerr << UTF8(sequence.first()->asString(context));
    }
    else if(len > 1) {
      cerr << "(";
      Sequence::const_iterator i = sequence.begin();
      Sequence::const_iterator end = sequence.end();
      while(i != end) {
        cerr << UTF8((*i)->asString(context));
        if(++i != end)
          cerr << ",";
      }
      cerr << ")";
    }
    cerr << endl;
  }

};

class QueryStore
{
public:
  typedef vector<XQQuery*>::iterator iterator;
  typedef vector<XQQuery*>::const_iterator const_iterator;

  QueryStore() {}
  ~QueryStore() {
    for(iterator i = begin(); i != end(); ++i)
      delete *i;
  }

  void push_back(XQQuery *query) {
    queries_.push_back(query);
  }
  const XQQuery *back() const {
    return queries_.back();
  }

  iterator begin() {
    return queries_.begin();
  }
  iterator end() {
    return queries_.end();
  }
  const_iterator begin() const {
    return queries_.begin();
  }
  const_iterator end() const {
    return queries_.end();
  }

private:
  QueryStore(const QueryStore &);
  QueryStore &operator=(const QueryStore &);

  vector<XQQuery*> queries_;
};

int main(int argc, char *argv[])
{
  // First we parse the command line arguments
  vector<char *> queries;

  const char* inputFile=NULL, *outputFile=NULL, *baseURIDir=NULL;
  map<string, char*> externalVars;
  bool quiet = false;
  int language = XQilla::XQUERY;
  bool xpathCompatible = false;
  int numberOfTimes = 1;
  bool printAST = false;

  XercesConfiguration xercesConf;
  FastXDMConfiguration fastConf;
  XQillaConfiguration *conf = &fastConf;

  for(int i = 1; i < argc; ++i) {
    if(*argv[i] == '-' && argv[i][2] == '\0' ){

      // -h option, print usage
      if(argv[i][1] == 'h') {
        usage(argv[0]);
        return 0;
      }
      else if(argv[i][1] == 'i') {
        ++i;
        if(i==argc)
        {
          cerr << "Missing argument to option 'i'" << endl;
          return 1;
        }
        inputFile=argv[i];
      }
      else if(argv[i][1] == 'b') {
        ++i;
        if(i==argc)
        {
          cerr << "Missing argument to option 'b'" << endl;
          return 1;
        }
        baseURIDir=argv[i];
      }
      else if(argv[i][1] == 'o') {
        ++i;
        if(i==argc)
        {
          cerr << "Missing argument to option 'o'" << endl;
          return 1;
        }
        outputFile=argv[i];
      }
      else if(argv[i][1] == 'n') {
        ++i;
        if(i==argc)
        {
          cerr << "Missing argument to option 'n'" << endl;
          return 1;
        }
        numberOfTimes=atoi(argv[i]);
      }
      else if(argv[i][1] == 'q') {
        quiet = true;
      }
      else if(argv[i][1] == 'f') {
        language |= XQilla::FULLTEXT;
      }
      else if(argv[i][1] == 'u') {
        language |= XQilla::UPDATE;
        conf = &xercesConf;
      }
      else if(argv[i][1] == 'p') {
        language |= XQilla::XPATH2;
      }
      else if(argv[i][1] == 'P') {
        // You can't use xpath 1 compatibility in
        // XQuery mode.
        language |= XQilla::XPATH2;
        xpathCompatible = true;
      }
      else if(argv[i][1] == 't') {
        printAST = true;
      }
      else if(argv[i][1] == 'v') {
        ++i;
        if((i + 1) >= argc) {
          cerr << "Missing argument to option 'v'" << endl;
          return 1;
        }
	externalVars[argv[i]] = argv[i + 1];
	++i;
      }
      else if(argv[i][1] == 'x') {
        conf = &xercesConf;
      }
      else {
        usage(argv[0]);
        return 1;
      }
    }
    else {
      queries.push_back(argv[i]);
    }
  }

  // Check for bad command line arguments
  if(queries.empty()) {
    usage(argv[0]);
    return 1;
  }

  // Create the XQilla object
  XQilla xqilla;
  MessageListenerImpl mlistener;

  int executionCount = 0;
  try {
    QueryStore parsedQueries;
    for(vector<char*>::iterator it1 = queries.begin();
        it1 != queries.end(); ++it1) {
      Janitor<DynamicContext> contextGuard(xqilla.createContext((XQilla::Language)language, conf));
      DynamicContext *context = contextGuard.get();

      // the DynamicContext has set the baseURI to the current file
      // we override to a user-specified value, or to the same directory as the
      // query (current file)
      if(baseURIDir != NULL) {
        context->setBaseURI(X(baseURIDir));
      }
      else {
        XMLCh *pwd = XMLPlatformUtils::getCurrentDirectory(context->getMemoryManager());
        if(pwd != NULL){
          XMLCh *baseURI = (XMLCh*)context->getMemoryManager()->allocate((XMLString::stringLen(pwd) + 10)*sizeof(XMLCh));
          XMLString::fixURI(pwd, baseURI);
          XMLString::catString(baseURI, &chForwardSlash);
          string queryPath(*it1);
          XMLUri base(baseURI);
          XMLUri resolved(&base, X(queryPath.c_str()));
          context->setBaseURI(resolved.getUriText());
        }
      }

      context->setXPath1CompatibilityMode(xpathCompatible);
      context->setMessageListener(&mlistener);

      parsedQueries.push_back(xqilla.parseFromURI(X(*it1), contextGuard.release()));

      if(printAST) {
        cerr << PrintAST::print(parsedQueries.back(), context) << endl;
      }
    }

    for(int count = numberOfTimes; count > 0; --count) {

      for(QueryStore::const_iterator it2 = parsedQueries.begin();
          it2 != parsedQueries.end(); ++it2) {

        Janitor<DynamicContext> dynamic_context((*it2)->createDynamicContext());
        if(inputFile != NULL) {
          // if an XML file was specified
          Sequence seq=dynamic_context->resolveDocument(X(inputFile), 0);
          if(!seq.isEmpty() && seq.first()->isNode()) {
            dynamic_context->setContextItem(seq.first());
            dynamic_context->setContextPosition(1);
            dynamic_context->setContextSize(1);
          }
        }

	// Set the external variable values
	map<string, char*>::iterator v = externalVars.begin();
	for(; v != externalVars.end(); ++v) {
		Item::Ptr value = dynamic_context->getItemFactory()->createString(X(v->second), dynamic_context.get());
		dynamic_context->setExternalVariable(X(v->first.c_str()), value);
	}

        time_t now;
        dynamic_context->setCurrentTime(time(&now));

        ++executionCount;

        if(quiet) {
          (*it2)->execute(dynamic_context.get())->toSequence(dynamic_context.get());
        }
        else {
          // use STDOUT if a file was not specified
          Janitor<XMLFormatTarget> target(0);
          if(outputFile != NULL) {
            target.reset(new LocalFileFormatTarget(outputFile));
          } else {
            target.reset(new StdOutFormatTarget());
          }

          EventSerializer writer("UTF-8", "1.1", target.get(), dynamic_context->getMemoryManager());
          writer.addNewlines(true);
          NSFixupFilter nsfilter(&writer, dynamic_context->getMemoryManager());
          (*it2)->execute(&nsfilter, dynamic_context.get());
        }
      }
    }
  }
  catch(XQException &e) {
    cerr << UTF8(e.getXQueryFile()) << ":" << e.getXQueryLine() << ":" << e.getXQueryColumn()
	 << ": error: " << UTF8(e.getError()) << endl;
//     cerr << "at " << e.getCppFile() << ":" << e.getCppLine() << endl;
    return 1;
  }
  catch(...) {
    cerr << "Caught unknown exception" << endl;
    return 1;
  }

  if(quiet) cout << "Executions: " << executionCount << endl;

  // clean up and exit
  return 0;
}

// print the usage message
void usage(const char *progname)
{
  const char *name = progname;
  while(*progname != 0) {
    if(*progname == '/' || *progname == '\\') {
      ++progname;
      name = progname;
    } else {
      ++progname;
    }
  }

  cerr << "Usage: " << name << " [options] <XQuery file>..." << endl << endl;
  cerr << "-b <baseURI>      : Set the base URI for the context" << endl;
  cerr << "-f                : Parse using W3C Full-Text extensions" << endl;
  cerr << "-u                : Parse using W3C Update extensions" << endl;
  cerr << "-h                : Show this display" << endl;
  cerr << "-i <file>         : Load XML document and bind it as the context item" << endl;
  cerr << "-n <number>       : Run the queries a number of times" << endl;
  cerr << "-o <file>         : Write the result to the specified file" << endl;
  cerr << "-p                : Parse in XPath 2 mode (default is XQuery mode)" << endl;
  cerr << "-P                : Parse in XPath 1.0 compatibility mode (default is XQuery mode)" << endl;
  cerr << "-q                : Quiet mode - no output" << endl;
  cerr << "-t                : Output an XML representation of the AST" << endl;
  cerr << "-v <name> <value> : Bind the name value pair as an external variable" << endl;
  cerr << "-x                : Use the Xerces-C data model (default is the FastXDM)" << endl;
}
