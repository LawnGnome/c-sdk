#
# Capture the output of running the axiom/tests as captured into files, such as:
#   ./agent/Darwin/gcc/x86_64/axiom/tests/test_base64.out
# This output will tell us how many tests passed/failed, and will contain information on the failures.
#
# Convert what is captured into .xml in a form that emulates what junit would produce.
#
# In this way the Hudson post-build action to read junit output can read, analyze and trend
# the output of the axiom/tests.
#
# See
#  http://stackoverflow.com/questions/4922867/junit-xml-format-specification-that-hudson-supports
#

#
# Much of this code was taken more or less wholesale by rrh from
#   http://www.natontesting.com/2012/05/25/rspec-junit-formatter-for-jenkins/
# without too much effort at normalizing the Ruby formatting,
# or understanding what is going on.
#
# This code is not shipped with our product.
# This code does not appear in the license file.
#

=begin

Copyright (c) 2012, Nathaniel Ritmeyer
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. Neither the name Nathaniel Ritmeyer nor the names of contributors to
this software may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
IS"" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=end

require "time"
require "builder"
require "rspec/core/formatters/base_formatter"
require "rspec/core/runner"
require "ostruct"
require "optparse"

class JUnit < RSpec::Core::Formatters::BaseFormatter
  def initialize output
    super output
    @test_results = []
  end

  def example_passed example
    @test_results << example
  end

  def example_failed example
    @test_results << example
  end

  def example_pending example
    @test_results << example
  end

  def failure_details_for example
    if false
      exception = example.metadata[:execution_result][:exception]
      exception.nil? ? "" : "#{exception.message}\n#{exception.to_s}"
    else
      failure_info = example.metadata[:execution_result][:failure_info]
      (failure_info.nil? || failure_info.size <= 0) ? "" : failure_info.to_s
    end
  end

  def full_name_for example
    test_name = ""
    current_example_group = example.metadata[:example_group]
    until current_example_group.nil? do
      test_name = "#{current_example_group[:description]}." + test_name
      current_example_group = current_example_group[:example_group]
    end
    test_name << example.metadata[:description]
  end

  def dump_summary duration, example_count, failure_count, pending_count
    builder = Builder::XmlMarkup.new :indent => 2
    builder.instruct! :xml, :version => "1.0", :encoding => "UTF-8"
    builder.testsuite :errors => 0, :failures => failure_count, :skipped => pending_count, :tests => example_count, :time => duration, :timestamp => Time.now.iso8601 do
      # builder.properties  # without any properties this output just <properties/> which seemed erroneous -rrh, 27Dec2013
      @test_results.each do |test|
        builder.testcase :classname => full_name_for(test), :name => test.metadata[:full_description], :time => test.metadata[:execution_result][:run_time] do
          # puts "status=#{test.metadata[:execution_result][:status]}"
          case test.metadata[:execution_result][:status]
          when "failed"
            builder.failure :message => "failed #{test.metadata[:full_description]}", :type => "failed" do
              builder.cdata! failure_details_for test
            end
          when "pending" then builder.skipped
          end
        end
      end
    end
    output.puts builder.target!
  end
end

class DuckTypedTestResult
  attr_accessor :metadata
  def initialize(full_description, status, run_time, failure_info)
    @metadata = {}
    @metadata[:execution_result] = {}

    @metadata[:full_description] = full_description
    @metadata[:description] = full_description

    @metadata[:execution_result][:run_time] = run_time
    @metadata[:execution_result][:status] = status
    # ex = Exception.new("some exception")
    # ex.set_backtrace(Kernel.caller)
    # @metadata[:execution_result][:exception] = ex

    @metadata[:execution_result][:failure_info] = failure_info
  end
end

def parse_args(args)
  options = OpenStruct.new
  options.uname = "Darwin"
  options.compiler = "gcc"
  options.arch = "x86_64"
  options.output_file_name = "stdout"
  all_options = OptionParser.new do |opts|
    opts.banner = "Usage: genjunit ...arguments..."

    opts.on("-o", "--output [STRING]", "name of output file") do |s|
      options.output_file_name = s
    end

    opts.on("-u", "--uname [STRING]", "canonical uname name, such as Darwin, Linux, ...") do |s|
      options.uname = s
    end

    opts.on("-c", "--compiler [STRING]", "canonical compiler short name, such as gcc, gxx, gccas, gccts, etc") do |s|
      options.compiler = s
    end

    opts.on("-a", "--arch [STRING]", "canonical architecture name, such as x86_64, etc") do |s|
      options.arch = s
    end

  end
  all_options.parse(args)
  return options
end

def crawl_test_output(options)
  # puts "file_name=#{options.output_file_name}"
  if ! options.output_file_name || options.output_file_name == "stdout"
    fd = STDOUT
  else
    fd = open(options.output_file_name, "w")
  end
  junit = JUnit.new(fd)

  example_count = 0
  failure_count = 0
  pending_count = 0

  filename_pattern = File.join("agent", options.uname, options.compiler, options.arch, "axiom", "tests", "*.out")

  Dir.glob(filename_pattern).each do |filename|
    test_name = nil

    example_count += 1

    pass_count = 0
    fail_count = 0
    ok_fail_count = 0

    fail_lines = []

    open(filename, "r").each_line do |line|
      line.strip!

      #
      # A failure line is of the form,
      #    FAIL [axiom/tests/test_base64.c:56]: TRUE check: valid character
      # Often, there is additional information following the FAIL line that we might want to pick up,
      # but the format of that may vary
      #
      re = %r{^\s*FAIL\s+\[.*:\d+\]:.*}
      m = re.match(line)
      if m
        fail_lines << line
      end

      #
      # An output line from axiom/tests/test_foo is of the form:
      # test_analytics_events: all  2229 tests passed  agent/Darwin/gcc/x86_64/axiom/tests/test_analytics_events
      #
      re = %r{^\s*([^:]+): all *(\d+) tests passed}

      m = re.match(line)
      if m
        test_name = m[1].to_s
        pass_count = m[2].to_i
      end

      #
      # An output line wherein we have some acceptable failures is of the form:
      #
      # test_rpm:   312 of   314 tests passed,     0 failed and     2 acceptably failed  agent/Darwin/gcc/x86_64/axiom/tests/test_rpm
      #
      re = %r{^\s*([^:]+): *(\d+) of *(\d+) tests passed, *(\d+) failed and *(\d+) acceptably failed}

      m = re.match(line)
      if m
        test_name = m[1].to_s
        pass_count = m[3].to_i
        fail_count = m[4].to_i
        ok_fail_count = m[5].to_i
      end
    end

    duration = 1  # made up

    if fail_count > 0
      failure_count += 1
      junit.example_failed(DuckTypedTestResult.new(test_name, "failed", duration, fail_lines))

    elsif ok_fail_count > 0
      pending_count += 1
      junit.example_pending(DuckTypedTestResult.new(test_name, "pending", duration, fail_lines))

    else
      junit.example_passed(DuckTypedTestResult.new(test_name, "passed", duration, fail_lines))
    end

  end

  duration = 10.0 # made up
  junit.dump_summary(duration, example_count, failure_count, pending_count)

end

if __FILE__ == $PROGRAM_NAME
  options = parse_args(ARGV)
  crawl_test_output(options)
end
