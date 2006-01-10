/*
 * Copyright (c) 2001-2006
 *     DecisionSoft Limited. All rights reserved.
 * Copyright (c) 2004-2006
 *     Progress Software Corporation. All rights reserved.
 * Copyright (c) 2004-2006
 *     Sleepycat Software. All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 *
 * $Id$
 */

#include <iostream>
#include <vector>

#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>

//XQilla includes
#include <xqilla/xqilla-simple.hpp>
#include <xqilla/context/impl/XQRemoteDebugger.hpp>

#if defined(XERCES_HAS_CPP_NAMESPACE)
XERCES_CPP_NAMESPACE_USE
#endif

#define QUERY_BUFFER_SIZE 32 * 1024
#define BASEURI_BUFFER_SIZE 2 * 1024

////////////////////////////
// function declarations  //
////////////////////////////

/** Print usage */
void usage(const char *progname);

class QueryStore
{
public:
  typedef std::vector<XQQuery*>::iterator iterator;
  typedef std::vector<XQQuery*>::const_iterator const_iterator;

  QueryStore() {}
  ~QueryStore() {
    for(iterator i = begin(); i != end(); ++i)
      delete *i;
  }

  void push_back(XQQuery *query) {
    queries_.push_back(query);
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

  std::vector<XQQuery*> queries_;
};

int main(int argc, char *argv[])
{
  // First we parse the command line arguments
  std::vector<char *> queries;

  const char* inputFile=NULL, *outputFile=NULL, *host=NULL, *baseURIDir=NULL;
  bool bRemoteDebug=false;
  bool quiet = false;
  bool xpathMode = false;
  bool xpathCompatible = false;
  int numberOfTimes = 1;

  for(int i = 1; i < argc; ++i) {
    if(*argv[i] == '-' && argv[i][2] == '\0' ){

      // -h option, print usage
      if(argv[i][1] == 'h') {
        usage(argv[0]);
        return 0;
      }
      else if(argv[i][1] == 'i') {
        i++;
        if(i==argc)
        {
          std::cerr << "Missing argument to option 'i'" << std::endl;
          return 1;
        }
        inputFile=argv[i];
      }
      else if(argv[i][1] == 'b') {
        i++;
        if(i==argc)
        {
          std::cerr << "Missing argument to option 'b'" << std::endl;
          return 1;
        }
        baseURIDir=argv[i];
      }
      else if(argv[i][1] == 'o') {
        i++;
        if(i==argc)
        {
          std::cerr << "Missing argument to option 'o'" << std::endl;
          return 1;
        }
        outputFile=argv[i];
      }
      else if(argv[i][1] == 'd') {
        bRemoteDebug=true;
        i++;
        if(i==argc)
        {
          std::cerr << "Missing argument to option 'd'" << std::endl;
          return 1;
        }
        host=argv[i];
      }
      else if(argv[i][1] == 'n') {
        i++;
        if(i==argc)
        {
          std::cerr << "Missing argument to option 'n'" << std::endl;
          return 1;
        }
        numberOfTimes=atoi(argv[i]);
      }
      else if(argv[i][1] == 'q') {
        quiet = true;
      }
      else if(argv[i][1] == 'p') {
        xpathMode = true;
      }
      else if(argv[i][1] == 'P') {
        // You can't use xpath 1 compatibility in
        // XQuery mode.
        xpathMode = true;
        xpathCompatible = true;
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

  int executionCount = 0;
  try {
    QueryStore parsedQueries;
    for(std::vector<char*>::iterator it1 = queries.begin();
        it1 != queries.end(); ++it1) {
      Janitor<DynamicContext> context(xqilla.createContext());

      // the DynamicContext has set the baseURI to the current file
      // we override to a user-specified value, or to the same directory as the
      // query (current file)
      if(baseURIDir != NULL) {
        context->setBaseURI(X(baseURIDir));
      }
      else {
        // FIXME assumes UTF8, Windows portability issues?
        char *pwd = ::getenv("PWD");
        if(pwd != NULL) {
          std::string queryPath(*it1);
          size_t idx = queryPath.rfind('/');
          if(idx != std::string::npos) {
            std::string baseURI = std::string("file:");
            baseURI += std::string(pwd);
            baseURI += std::string(1, '/');
            baseURI += queryPath.substr(0, idx);
            baseURI += std::string(1, '/');
            context->setBaseURI(X(baseURI.c_str()));
          }
        }
      }

      context->setXPath1CompatibilityMode(xpathCompatible);

      if(bRemoteDebug) {
        context->setDebugCallback(new (context->getMemoryManager()) XQRemoteDebugger(X(host), context->getMemoryManager()));
        context->enableDebugging(true);
      }

      if(xpathMode) {
        parsedQueries.push_back(xqilla.parseXPath2FromURI(X(*it1), context.release()));
      }
      else {
        parsedQueries.push_back(xqilla.parseXQueryFromURI(X(*it1), context.release()));
      }
    }

    for(int count = numberOfTimes; count > 0; --count) {

      for(QueryStore::const_iterator it2 = parsedQueries.begin();
          it2 != parsedQueries.end(); ++it2) {

        Janitor<DynamicContext> dynamic_context((*it2)->createDynamicContext());
        if(inputFile != NULL) {
          // if an XML file was specified
          Sequence seq=dynamic_context->resolveDocument(X(inputFile));
          if(!seq.isEmpty() && seq.first()->isNode()) {
            dynamic_context->setContextItem(seq.first());
            dynamic_context->setContextPosition(1);
            dynamic_context->setContextSize(1);
          }
        }
        time_t now;
        dynamic_context->setCurrentTime(time(&now));

        Result result = (*it2)->execute(dynamic_context.get()).toSequence(dynamic_context.get());
        ++executionCount;

        if(outputFile != NULL || !quiet) {
          // use STDOUT if a file was not specified
          Janitor<XMLFormatTarget> target(0);
          if(outputFile != NULL) {
            target.reset(new LocalFileFormatTarget(outputFile));
          } else {
            target.reset(new StdOutFormatTarget());
          }

          // assume UTF8
          XMLFormatter formatter(XMLUni::fgUTF8EncodingString, target.get());
          formatter << XMLFormatter::NoEscapes << XMLFormatter::UnRep_CharRef;

          Item::Ptr item;
          while((item = result.next(dynamic_context.get())) != NULLRCP) {
            formatter << item->asString(dynamic_context.get()) << (XMLCh)'\n';
          }
        }

      }
    }
  }
  catch(XQException &e) {
    std::cerr << "Caught XQException:" << std::endl << UTF8(e.getError()) << std::endl;
    std::cerr << "at " << UTF8(e.getXQueryFile()) << ":" << e.getXQueryLine() << ":" << e.getXQueryColumn() << std::endl;
    return 1;
  }
  catch(...) {
    std::cerr << "Caught unknown exception" << std::endl;
    return 1;
  }

  if(quiet) std::cout << "Executions: " << executionCount << std::endl;

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

  std::cerr << "Usage: " << name << " [options] <XQuery file>..." << std::endl << std::endl;
  std::cerr << "-b <baseURI>   : Set the base URI for the context" << std::endl;
  std::cerr << "-d <host:port> : Enable remote debugging" << std::endl;
  std::cerr << "-h             : Show this display" << std::endl;
  std::cerr << "-i <file>      : Load XML document and bind it as the context item" << std::endl;
  std::cerr << "-n <number>    : Run the queries a number of times" << std::endl;
  std::cerr << "-o <file>      : Write the result to the specified file" << std::endl;
  std::cerr << "-p             : Parse in XPath 2 mode (default is XQuery mode)" << std::endl;
  std::cerr << "-P             : Parse in XPath 1.0 compatibility mode (default is XQuery mode)" << std::endl;
  std::cerr << "-q             : Quiet mode - no output" << std::endl;
}
