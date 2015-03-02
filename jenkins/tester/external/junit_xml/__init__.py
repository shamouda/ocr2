#!/usr/bin/env python
from lxml import etree
import re, sys

"""
Based on the following understanding of what Jenkins can parse for JUnit XML files.

<?xml version="1.0" encoding="utf-8"?>
<testsuites errors="1" failures="1" tests="4" time="45">
    <testsuite errors="1" failures="1" hostname="localhost" id="0" name="base_test_1"
               package="testdb" tests="4" timestamp="2012-11-15T01:02:29">
        <properties>
            <property name="assert-passed" value="1"/>
        </properties>
        <testcase classname="testdb.directory" name="001-passed-test" time="10"/>
        <testcase classname="testdb.directory" name="002-failed-test" time="20">
            <failure message="Assertion FAILED: some failed assert" type="failure">
                the output of the testcase
            </failure>
        </testcase>
        <testcase classname="package.directory" name="003-errord-test" time="15">
            <error message="Assertion ERROR: some error assert" type="error">
                the output of the testcase
            </error>
        </testcase>
	<testcase classname="package.directory" name="003-skipped-test" time="0">
	    <skipped message="SKIPPED Test" type="skipped">
                the output of the testcase
            </skipped>
	</testcase>
        <testcase classname="testdb.directory" name="003-passed-test" time="10">
            <system-out>
                I am system output
            </system-out>
            <system-err>
                I am the error output
            </system-err>
        </testcase>
    </testsuite>
</testsuites>
"""


class TestSuite(object):
    """Suite of test cases"""

    def __init__(self, name, test_cases=None, hostname=None, id=None,\
                 package=None, timestamp=None, properties=None):
        self.name = name
        if not test_cases:
            test_cases = []
        try:
            iter(test_cases)
        except TypeError:
            raise Exception('test_cases must be a list of test cases')
        self.test_cases = test_cases
        self.hostname = hostname
        self.id = id
        self.package = package
        self.timestamp = timestamp
        self.properties = properties

    def add_test_case(self, test_case):
        self.test_cases.append(test_case)

    def merge_cases(self, testSuite):
        self.test_cases.extend(testSuite.test_cases)

    def writeOut(self, xmlfile, doHierarchical):
        """Builds the XML document for the JUnit test suite"""
        # build the test suite element
        test_suite_attributes = dict()
        test_suite_attributes['name'] = str(self.name)
        test_suite_attributes['failures'] = str(sum(c.failure_count() for c in self.test_cases))
        test_suite_attributes['errors'] = str(sum(c.error_count() for c in self.test_cases))
	test_suite_attributes['skipped'] = str(sum(c.skipped_count() for c in self.test_cases))
        test_suite_attributes['time'] = str(sum(c.elapsed_time() for c in self.test_cases))
        test_suite_attributes['tests'] = str(sum(c.test_count() for c in self.test_cases))

        if self.hostname:
            test_suite_attributes['hostname'] = str(self.hostname)
        if self.id:
            test_suite_attributes['id'] = str(self.id)
        if self.package:
            test_suite_attributes['package'] = str(self.package)
        if self.timestamp:
            test_suite_attributes['timestamp'] = str(self.timestamp)

        # Write out the test suite, one test-case at a time
        with xmlfile.element("testsuite", attrib=test_suite_attributes):
            # add any properties
            if self.properties:
                props_element = etree.Element("properties")
                for k, v in self.properties.items():
                    attrs = {'name': str(k), 'value': str(v)}
                    props_element.append(etree.Element("property", attrib=attrs))
                xmlfile.write(props_element)
            for c in self.test_cases:
                c.writeOut(xmlfile, self.package if doHierarchical else None)

    @staticmethod
    def to_file(file_name, test_suites, encoding=None, doHierarchical=False):
        """Writes the JUnit XML document to file"""
        with etree.xmlfile(file_name, encoding=encoding) as xf:
            xf.write_declaration(standalone=True)
            with xf.element("testsuites"):
                for ts in test_suites:
                    ts.writeOut(xf, doHierarchical)

    @staticmethod
    def _clean_illegal_xml_chars(string_to_clean):
        """Removes any illegal unicode characters from the given XML string"""
        # see http://stackoverflow.com/questions/1707890/fast-way-to-filter-illegal-xml-unicode-chars-in-python
        illegal_unichrs = [(0x00, 0x08), (0x0B, 0x1F), (0x7F, 0x84), (0x86, 0x9F),
                           (0xD800, 0xDFFF), (0xFDD0, 0xFDDF), (0xFFFE, 0xFFFF),
                           (0x1FFFE, 0x1FFFF), (0x2FFFE, 0x2FFFF), (0x3FFFE, 0x3FFFF),
                           (0x4FFFE, 0x4FFFF), (0x5FFFE, 0x5FFFF), (0x6FFFE, 0x6FFFF),
                           (0x7FFFE, 0x7FFFF), (0x8FFFE, 0x8FFFF), (0x9FFFE, 0x9FFFF),
                           (0xAFFFE, 0xAFFFF), (0xBFFFE, 0xBFFFF), (0xCFFFE, 0xCFFFF),
                           (0xDFFFE, 0xDFFFF), (0xEFFFE, 0xEFFFF), (0xFFFFE, 0xFFFFF),
                           (0x10FFFE, 0x10FFFF)]

        illegal_ranges = ["%s-%s" % (unichr(low), unichr(high))
                          for (low, high) in illegal_unichrs
                          if low < sys.maxunicode]

        illegal_xml_re = re.compile(u'[%s]' % u''.join(illegal_ranges))
        return illegal_xml_re.sub('', string_to_clean)

class TestCasesFile(object):
    """A file containing a whole bunch of test cases of the same class"""

    def __init__(self, filePath):
        self._filePath = filePath
        for event, elem in etree.iterparse(filePath, events=("start",), tag="testsuite"):
            # We should really only have one element (the first one parsed)
            self._failure_count = int(elem.get('failures', 0))
            self._error_count = int(elem.get('errors', 0))
            self._skipped_count = int(elem.get('skipped', 0))
            self._time = float(elem.get('time', 0))
            self._tests = int(elem.get('tests', 0))
            elem.clear()
            break

    def writeOut(self, xmlfile, packageName):
        for event, elem in etree.iterparse(self._filePath, events=("end",), tag="testcase"):
            if packageName is not None:
                oldClassName = elem.get('classname', '<unknown>')
                elem.set('classname', packageName + '.' + oldClassName)
            xmlfile.write(elem)
            elem.clear()

    def failure_count(self):
        return self._failure_count

    def error_count(self):
        return self._error_count

    def skipped_count(self):
        return self._skipped_count

    def elapsed_time(self):
        return self._time

    def test_count(self):
        return self._tests


class TestCase(object):
    """A JUnit test case with a result and possibly some stdout or stderr"""

    def __init__(self, name, classname=None, elapsed_sec=None, stdout=None, stderr=None):
        self.name = name
        self.elapsed_sec = elapsed_sec
        self.stdout = TestSuite._clean_illegal_xml_chars(stdout)
        self.stderr = TestSuite._clean_illegal_xml_chars(stderr)
        self.classname = classname
        self.error_message = None
        self.error_output = None
        self.failure_message = None
        self.failure_output = None
        self.skipped_message = None
        self.skipped_output = None

    def writeOut(self, xmlfile, packageName):
        # Build the element and output it
        test_case_attributes = dict()
        test_case_attributes['name'] = str(self.name)
        if self.elapsed_sec:
            test_case_attributes['time'] = "%f" % self.elapsed_sec
        if self.classname:
            test_case_attributes['classname'] = str(self.classname)
        else:
            test_case_attributes['classname'] = '<unknown>'
        if packageName is not None:
            test_case_attributes['classname'] = packageName + '.' + test_case_attributes['classname']

        # Build the root element
        test_case_element = etree.Element("testcase", attrib=test_case_attributes)

        # failures
        if self.failure_count() == 1:
            attrs = {'type': 'failure'}
            if self.failure_message:
                attrs['message'] = self.failure_message
            failure_element = etree.Element("failure", attrib=attrs)
            if self.failure_output:
                failure_element.text = self.failure_output
            test_case_element.append(failure_element)

        # errors
        if self.error_count() == 1:
            attrs = {'type': 'error'}
            if self.error_message:
                attrs['message'] = self.error_message
            error_element = etree.Element("error", attrib=attrs)
            if self.error_output:
                error_element.text = self.error_output
            test_case_element.append(error_element)

        # skippeds
        if self.skipped_count() == 1:
            attrs = {'type': 'skipped'}
            if self.skipped_message:
                attrs['message'] = self.skipped_message
            skipped_element = etree.Element("skipped", attrib=attrs)
            if self.error_output:
                skipped_element.text = self.skipped_output
            test_case_element.append(skipped_element)

        # test stdout
        if self.stdout:
            stdout_element = etree.Element("system-out")
            stdout_element.text = self.stdout
            test_case_element.append(stdout_element)

        # test stderr
        if self.stderr:
            stderr_element = etree.Element("system-err")
            stderr_element.text = self.stderr
            test_case_element.append(stderr_element)

        # Now write it out
        xmlfile.write(test_case_element)
        test_case_element.clear()

    def add_error_info(self, message=None, output=None):
        """Adds an error message, output, or both to the test case"""
        if message:
            self.error_message = message
        if output:
            self.error_output = TestSuite._clean_illegal_xml_chars(output)

    def add_failure_info(self, message=None, output=None):
        """Adds a failure message, output, or both to the test case"""
        if message:
            self.failure_message = message
        if output:
            self.failure_output = TestSuite._clean_illegal_xml_chars(output)

    def add_skipped_info(self, message=None, output=None):
        """Adds a skipped message, output, or both to the test case"""
        if message:
            self.skipped_message = message
        if output:
            self.skipped_output = TestSuite._clean_illegal_xml_chars(output)

    def failure_count(self):
        """returns 1 if this test case is a failure"""
        return 1 if self.failure_output or self.failure_message else 0

    def error_count(self):
        """returns 1 if this test case is an error"""
        return 1 if self.error_output or self.error_message else 0

    def skipped_count(self):
        """returns 1 if this test case has been skipped"""
        return 1 if self.skipped_output or self.skipped_message else 0

    def elapsed_time(self):
        return self.elapsed_sec

    def test_count(self):
        return 1
