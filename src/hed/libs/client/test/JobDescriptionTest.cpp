// -*- indent-tabs-mode: nil -*-
#include <iostream>
#include <string>

#include <cppunit/extensions/HelperMacros.h>

#include <arc/Logger.h>
#include <arc/client/JobDescription.h>

class JobDescriptionTest
  : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(JobDescriptionTest);
  CPPUNIT_TEST(Print);
  CPPUNIT_TEST(TestArcJSDL);
  CPPUNIT_TEST(TestXRSL);
  CPPUNIT_TEST_SUITE_END();

public:
  JobDescriptionTest()
    : logger(Arc::Logger::getRootLogger(), "JobDescriptionTest"),
      logcerr(std::cerr)
    {}
  void setUp();
  void tearDown();
  void Print();
  void TestArcJSDL();
  void TestXRSL();

private:
  Arc::JobDescription job;
  Arc::JobDescription arcJob;
  Arc::JobDescription xrslJob;
  Arc::JobDescription jdlJob;
  Arc::JobDescription jsdlJob;

  Arc::LogStream logcerr;
  Arc::Logger logger;
};


void JobDescriptionTest::setUp() {
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  job.Executable = "executable";
  job.Input = "input-file";
  job.Output = "output-file";
  job.Error = "error-file";
  
  {
  Arc::FileType file;
  file.Name = "1-Create-Delete";
  file.type = Arc::FileType::LOCAL;
  file.KeepData = false;
  job.File.push_back(file);
  }
  
  {
  Arc::FileType file;
  file.Name = "2-Download-Delete";
  file.type = Arc::FileType::SOURCE;
  file.Source.URI = "http://example.com/" + file.Name;
  file.KeepData = false;
  job.File.push_back(file);
  }

  {
  Arc::FileType file;
  file.Name = "3-Upload-Delete";
  file.type = Arc::FileType::SOURCE;
  file.KeepData = false;
  job.File.push_back(file);
  }
  
  {
  Arc::FileType file;
  file.Name = "4-Create-Download";
  file.type = Arc::FileType::LOCAL;
  file.KeepData = true;
  job.File.push_back(file);
  }
    
  {
  Arc::FileType file;
  file.Name = "5-Download-Download";
  file.type = Arc::FileType::SOURCE;
  file.Source.URI = "http://example.com/" + file.Name;
  file.KeepData = true;
  job.File.push_back(file);
  }

  {
  Arc::FileType file;
  file.Name = "6-Upload-Download";
  file.type = Arc::FileType::SOURCE;
  file.KeepData = true;
  job.File.push_back(file);
  }

  {
  Arc::FileType file;
  file.Name = "7-Create-Upload";
  file.type = Arc::FileType::TARGET;
  file.Target.URI = "http://example.com/" + file.Name;
  file.KeepData = false;
  job.File.push_back(file);
  }
  
  {
  Arc::FileType file;
  file.Name = "8-Download-Upload";
  file.type = (Arc::FileType::SOURCE|Arc::FileType::TARGET);
  file.Target.URI = "http://example.com/" + file.Name;
  file.Source.URI = "http://example.com/" + file.Name;
  file.KeepData = false;
  job.File.push_back(file);
  }
  
  {
  Arc::FileType file;
  file.Name = "9-Upload-Upload";
  file.type = (Arc::FileType::SOURCE|Arc::FileType::TARGET);
  file.Target.URI = "http://example.com/" + file.Name;
  file.KeepData = false;
  job.File.push_back(file);
  }
}


void JobDescriptionTest::tearDown() {
}

void JobDescriptionTest::Print() {
  job.Print(true);
}

void JobDescriptionTest::TestArcJSDL() {
  arcJob.Parse(job.UnParse("posixjsdl"));
  CPPUNIT_ASSERT(arcJob);
  CPPUNIT_ASSERT_EQUAL(arcJob, job);
}

void JobDescriptionTest::TestXRSL() {
  xrslJob.Parse(job.UnParse("xrsl"));
  xrslJob.Print(true);
  Arc::Logger::getRootLogger().setThreshold(Arc::VERBOSE);
  CPPUNIT_ASSERT(xrslJob);
  CPPUNIT_ASSERT_EQUAL(xrslJob, job);
}

/*
void URLTest::TestGsiftpUrl() {
  CPPUNIT_ASSERT_EQUAL(gsiftpurl->Protocol(), std::string("gsiftp"));
  CPPUNIT_ASSERT(gsiftpurl->Username().empty());
  CPPUNIT_ASSERT(gsiftpurl->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(gsiftpurl->Host(), std::string("hathi.hep.lu.se"));
  CPPUNIT_ASSERT_EQUAL(gsiftpurl->Port(), 2811);
  CPPUNIT_ASSERT_EQUAL(gsiftpurl->Path(), std::string("public/test.txt"));
  CPPUNIT_ASSERT(gsiftpurl->HTTPOptions().empty());
  CPPUNIT_ASSERT(gsiftpurl->Options().empty());
  CPPUNIT_ASSERT(gsiftpurl->Locations().empty());
}


void URLTest::TestLdapUrl() {
  CPPUNIT_ASSERT_EQUAL(ldapurl->Protocol(), std::string("ldap"));
  CPPUNIT_ASSERT(ldapurl->Username().empty());
  CPPUNIT_ASSERT(ldapurl->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(ldapurl->Host(), std::string("grid.uio.no"));
  CPPUNIT_ASSERT_EQUAL(ldapurl->Port(), 389);
  CPPUNIT_ASSERT_EQUAL(ldapurl->Path(), std::string("mds-vo-name=local, o=grid"));
  CPPUNIT_ASSERT(ldapurl->HTTPOptions().empty());
  CPPUNIT_ASSERT(ldapurl->Options().empty());
  CPPUNIT_ASSERT(ldapurl->Locations().empty());
}


void URLTest::TestHttpUrl() {
  CPPUNIT_ASSERT_EQUAL(httpurl->Protocol(), std::string("http"));
  CPPUNIT_ASSERT(httpurl->Username().empty());
  CPPUNIT_ASSERT(httpurl->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(httpurl->Host(), std::string("www.nordugrid.org"));
  CPPUNIT_ASSERT_EQUAL(httpurl->Port(), 80);
  CPPUNIT_ASSERT_EQUAL(httpurl->Path(), std::string("monitor.php"));

  std::map<std::string, std::string> httpmap = httpurl->HTTPOptions();
  CPPUNIT_ASSERT_EQUAL((int)httpmap.size(), 2);

  std::map<std::string, std::string>::iterator mapit = httpmap.begin();
  CPPUNIT_ASSERT_EQUAL(mapit->first, std::string("debug"));
  CPPUNIT_ASSERT_EQUAL(mapit->second, std::string("2"));

  mapit++;
  CPPUNIT_ASSERT_EQUAL(mapit->first, std::string("sort"));
  CPPUNIT_ASSERT_EQUAL(mapit->second, std::string("yes"));

  CPPUNIT_ASSERT(httpurl->Options().empty());
  CPPUNIT_ASSERT(httpurl->Locations().empty());
}


void URLTest::TestFileUrl() {
  CPPUNIT_ASSERT_EQUAL(fileurl->Protocol(), std::string("file"));
  CPPUNIT_ASSERT(fileurl->Username().empty());
  CPPUNIT_ASSERT(fileurl->Passwd().empty());
  CPPUNIT_ASSERT(fileurl->Host().empty());
  CPPUNIT_ASSERT_EQUAL(fileurl->Port(), -1);
  CPPUNIT_ASSERT_EQUAL(fileurl->Path(), std::string("/home/grid/runtime/TEST-ATLAS-8.0.5"));
  CPPUNIT_ASSERT(fileurl->HTTPOptions().empty());
  CPPUNIT_ASSERT(fileurl->Options().empty());
  CPPUNIT_ASSERT(fileurl->Locations().empty());
}


void URLTest::TestLdapUrl2() {
  CPPUNIT_ASSERT_EQUAL(ldapurl2->Protocol(), std::string("ldap"));
  CPPUNIT_ASSERT(ldapurl2->Username().empty());
  CPPUNIT_ASSERT(ldapurl2->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(ldapurl2->Host(), std::string("grid.uio.no"));
  CPPUNIT_ASSERT_EQUAL(ldapurl2->Port(), 389);
  CPPUNIT_ASSERT_EQUAL(ldapurl2->Path(), std::string("mds-vo-name=local, o=grid"));
  CPPUNIT_ASSERT(ldapurl2->HTTPOptions().empty());
  CPPUNIT_ASSERT(ldapurl2->Options().empty());
  CPPUNIT_ASSERT(ldapurl2->Locations().empty());
}


void URLTest::TestOptUrl() {
  CPPUNIT_ASSERT_EQUAL(opturl->Protocol(), std::string("gsiftp"));
  CPPUNIT_ASSERT(opturl->Username().empty());
  CPPUNIT_ASSERT(opturl->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(opturl->Host(), std::string("hathi.hep.lu.se"));
  CPPUNIT_ASSERT_EQUAL(opturl->Port(), 2811);
  CPPUNIT_ASSERT_EQUAL(opturl->Path(), std::string("public/test.txt"));
  CPPUNIT_ASSERT(opturl->HTTPOptions().empty());
  CPPUNIT_ASSERT(opturl->Locations().empty());

  std::map<std::string, std::string> options = opturl->Options();
  CPPUNIT_ASSERT_EQUAL((int)options.size(), 2);

  std::map<std::string, std::string>::iterator mapit = options.begin();
  CPPUNIT_ASSERT_EQUAL(mapit->first, std::string("ftpthreads"));
  CPPUNIT_ASSERT_EQUAL(mapit->second, std::string("10"));

  mapit++;
  CPPUNIT_ASSERT_EQUAL(mapit->first, std::string("upload"));
  CPPUNIT_ASSERT_EQUAL(mapit->second, std::string("yes"));
}


void URLTest::TestFtpUrl() {
  CPPUNIT_ASSERT_EQUAL(ftpurl->Protocol(), std::string("ftp"));
  CPPUNIT_ASSERT_EQUAL(ftpurl->Username(), std::string("user"));
  CPPUNIT_ASSERT_EQUAL(ftpurl->Passwd(), std::string("secret"));
  CPPUNIT_ASSERT_EQUAL(ftpurl->Host(), std::string("ftp.nordugrid.org"));
  CPPUNIT_ASSERT_EQUAL(ftpurl->Port(), 21);
  CPPUNIT_ASSERT_EQUAL(ftpurl->Path(), std::string("pub/files/guide.pdf"));
  CPPUNIT_ASSERT(ftpurl->HTTPOptions().empty());
  CPPUNIT_ASSERT(ftpurl->Options().empty());
  CPPUNIT_ASSERT(ftpurl->Locations().empty());
}


void URLTest::TestRlsUrl() {
  CPPUNIT_ASSERT_EQUAL(rlsurl->Protocol(), std::string("rls"));
  CPPUNIT_ASSERT(rlsurl->Username().empty());
  CPPUNIT_ASSERT(rlsurl->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(rlsurl->Host(), std::string("rls.nordugrid.org"));
  CPPUNIT_ASSERT_EQUAL(rlsurl->Port(), 39281);
  CPPUNIT_ASSERT_EQUAL(rlsurl->Path(), std::string("test.txt"));
  CPPUNIT_ASSERT(rlsurl->HTTPOptions().empty());
  CPPUNIT_ASSERT(rlsurl->Options().empty());
  CPPUNIT_ASSERT(rlsurl->Locations().empty());
}


void URLTest::TestRlsUrl2() {
  CPPUNIT_ASSERT_EQUAL(rlsurl2->Protocol(), std::string("rls"));
  CPPUNIT_ASSERT(rlsurl2->Username().empty());
  CPPUNIT_ASSERT(rlsurl2->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(rlsurl2->Host(), std::string("rls.nordugrid.org"));
  CPPUNIT_ASSERT_EQUAL(rlsurl2->Port(), 39281);
  CPPUNIT_ASSERT_EQUAL(rlsurl2->Path(), std::string("test.txt"));
  CPPUNIT_ASSERT(rlsurl2->HTTPOptions().empty());
  CPPUNIT_ASSERT(rlsurl2->Options().empty());

  std::list<Arc::URLLocation> locations = rlsurl2->Locations();
  CPPUNIT_ASSERT_EQUAL((int)locations.size(), 2);

  std::list<Arc::URLLocation>::iterator urlit = locations.begin();
  CPPUNIT_ASSERT_EQUAL(urlit->str(), std::string("gsiftp://hagrid.it.uu.se:2811/storage/test.txt"));

  urlit++;
  CPPUNIT_ASSERT_EQUAL(urlit->str(), std::string("http://www.nordugrid.org:80/files/test.txt"));
}


void URLTest::TestRlsUrl3() {
  CPPUNIT_ASSERT_EQUAL(rlsurl3->Protocol(), std::string("rls"));
  CPPUNIT_ASSERT(rlsurl3->Username().empty());
  CPPUNIT_ASSERT(rlsurl3->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(rlsurl3->Host(), std::string("rls.nordugrid.org"));
  CPPUNIT_ASSERT_EQUAL(rlsurl3->Port(), 39281);
  CPPUNIT_ASSERT_EQUAL(rlsurl3->Path(), std::string("test.txt"));
  CPPUNIT_ASSERT(rlsurl3->HTTPOptions().empty());

  std::map<std::string, std::string> options = rlsurl3->Options();
  CPPUNIT_ASSERT_EQUAL((int)options.size(), 1);

  std::map<std::string, std::string>::iterator mapit = options.begin();
  CPPUNIT_ASSERT_EQUAL(mapit->first, std::string("local"));
  CPPUNIT_ASSERT_EQUAL(mapit->second, std::string("yes"));

  options = rlsurl3->CommonLocOptions();
  CPPUNIT_ASSERT_EQUAL((int)options.size(), 1);

  mapit = options.begin();
  CPPUNIT_ASSERT_EQUAL(mapit->first, std::string("exec"));
  CPPUNIT_ASSERT_EQUAL(mapit->second, std::string("yes"));

  std::list<Arc::URLLocation> locations = rlsurl3->Locations();
  CPPUNIT_ASSERT_EQUAL((int)locations.size(), 2);

  std::list<Arc::URLLocation>::iterator urlit = locations.begin();
  CPPUNIT_ASSERT_EQUAL(urlit->fullstr(), std::string("gsiftp://hagrid.it.uu.se:2811;threads=10/storage/test.txt"));

  urlit++;
  CPPUNIT_ASSERT_EQUAL(urlit->fullstr(), std::string("http://www.nordugrid.org:80;cache=no/files/test.txt"));
}

void URLTest::TestLfcUrl() {
  CPPUNIT_ASSERT_EQUAL(std::string("lfc"), lfcurl->Protocol());
  CPPUNIT_ASSERT(lfcurl->Username().empty());
  CPPUNIT_ASSERT(lfcurl->Passwd().empty());
  CPPUNIT_ASSERT_EQUAL(std::string("atlaslfc.nordugrid.org"), lfcurl->Host());
  CPPUNIT_ASSERT_EQUAL(5010, lfcurl->Port());
  CPPUNIT_ASSERT_EQUAL(std::string("/grid/atlas/file1"), lfcurl->Path());
  CPPUNIT_ASSERT(lfcurl->HTTPOptions().empty());

  std::map<std::string, std::string> options = lfcurl->Options();
  CPPUNIT_ASSERT_EQUAL(1, (int)options.size());

  std::map<std::string, std::string>::iterator mapit = options.begin();
  CPPUNIT_ASSERT_EQUAL(std::string("cache"), mapit->first);
  CPPUNIT_ASSERT_EQUAL(std::string("no"), mapit->second);

  CPPUNIT_ASSERT(lfcurl->CommonLocOptions().empty());

  CPPUNIT_ASSERT(lfcurl->Locations().empty());

  CPPUNIT_ASSERT_EQUAL(3, (int)lfcurl->MetaDataOptions().size());
  CPPUNIT_ASSERT_EQUAL(std::string("7d36da04-430f-403c-adfb-540b27506cfa"), lfcurl->MetaDataOption("guid"));
  CPPUNIT_ASSERT_EQUAL(std::string("ad"), lfcurl->MetaDataOption("checksumtype"));
  CPPUNIT_ASSERT_EQUAL(std::string("12345678"), lfcurl->MetaDataOption("checksumvalue"));
}
*/
CPPUNIT_TEST_SUITE_REGISTRATION(JobDescriptionTest);
