#! /usr/bin/env ruby

#
# This script merges the output of separate stashed gcov runs.
# We need this script to merge gcov runs because gcov does not seem
# to handle #include "foo.c" correctly.
#
# This script is given a list of directories that are presumed to represent
# all of the gcov generated annotated source files compiled into a single executed binary.
#
# Then, these gcov produced .gdov files coming from the same source file are merged together
# under the sum or max operator.
#
# The result of the merge is written to the directory "./gcov_aggregate",
# with one file for each of the contributing source files,
# taken across all of the analyzed binaries.
#
# Each line of output with an execution count > 0 contains
# the name of the test that executed the line the most;
# the count of the number of times that line was executed,
#  summed over all executions;
# the original source line.
#
# Author: Robert R. Henry 
#
require "set"
require "optparse"
require "ostruct"

class Count
  attr_accessor :handle
  attr_accessor :input_file_name, :lineno
  attr_accessor :sum_count
  attr_accessor :max_count
  attr_accessor :text_line

  def initialize(handle, input_file_name, lineno, count_string, text_line)
    @handle = handle
    @input_file_name = input_file_name
    @lineno = lineno
    if count_string == "-"
      @sum_count = -1
    elsif count_string == "####"
      @sum_count = 0
    else
      @sum_count = count_string.to_i
    end
    @text_line = text_line
    @max_count = @sum_count
  end

  def isexecutable?
    sum_count >= 0
  end

  def executed?
    sum_count > 0
  end

  def locus
    "#{self.input_file_name}:#{self.lineno}"
  end

  def merge(other)
    # Lines marked with - are either uncompilable (comments, say)
    # or uncompiled (non used inlined functions).
    if self.sum_count < 0 && other.sum_count >= 0
      self.sum_count = 0
    elsif self.sum_count >= 0 && other.sum_count < 0
      other.sum_count = 0
    elsif self.sum_count < 0 && other.sum_count < 0
      return
    end

    if self.text_line != other.text_line
      if /EOF/ =~ self.text_line || /EOF/ =~ other.text_line
        #
        # somehow output of gcov is horked (can'tfind source?)
        #
      else
        STDERR.puts "line skew failure: #{self.text_line} (#{self.locus}) vs #{other.text_line} (#{other.locus})"
      end
    end

    self.sum_count += other.sum_count

    if self.max_count < other.max_count
      self.max_count = other.max_count
      self.handle = other.handle
      self.input_file_name = other.input_file_name
      self.lineno = other.lineno
    end

  end

  def to_s
    if self.sum_count == 0
      handle_string = ""
      count_string = "####"
    elsif self.sum_count < 0
      handle_string = ""
      count_string = "-"
    else
      handle_string = self.handle
      count_string = self.sum_count.to_s
    end
    return "%30s %8s: %s" % [handle_string, count_string, self.text_line]
  end

end

class CoverageSummary
  attr_accessor :unexecutable, :notexecuted, :executed
  def initialize()
    @unexecutable = 0
    @notexecuted = 0
    @executed = 0
  end

  def merge(aCount)
    if aCount.isexecutable?
      if aCount.executed?
        @executed += 1
      else
        @notexecuted += 1
      end
    else
      @unexecutable += 1
    end
  end

  def fraction_covered
    total = executed + notexecuted
    (total > 0) ? (executed * 100.0 / total) : 0.0
  end

  def summarize(options, fd, namestring)
    total = executed + notexecuted
    if options.verbose
      fd.puts "%30s %6d %-6d %6.2f%%" % [namestring, executed, total, fraction_covered]
    end
  end

end

def aggregate_many_gcov_files(options, argv)
  source_file_to_dir_names = {}
  argv.each do |dir_name|
    Dir.glob("#{dir_name}/*.gcov").each do |file_name|
      source_file = File.basename(file_name)
      source_file_to_dir_names[source_file] ||= Set[]
      source_file_to_dir_names[source_file] << dir_name
    end
  end

  begin
    Dir.mkdir(options.output_dir_name)
  rescue SystemCallError
  end

  summaries = {}
  source_file_to_dir_names.each_pair do |source_file, dir_names|
    lineno_to_count = {}
    output_file_name = File.join(options.output_dir_name, source_file)
    # puts "output file: #{output_file_name}"
    output_file = open(output_file_name, "w")
    dir_names.each do |dir_name|
      input_file_name = File.join(dir_name, source_file)
      handle = dir_name.split(".gcov_stash")[0]
      lineno = -1
      IO.foreach(input_file_name) do |line|
        lineno += 1
        if /^function/ =~ line  # ouput format from solaris gcc3.3
          lineno -= 1
          next
        end
        parts = line.split(":", 2)
        count_string = parts[0].strip
        text_line = parts[1].rstrip
        count = Count.new(handle, input_file_name, lineno, count_string, text_line)
        if not lineno_to_count[lineno]
          lineno_to_count[lineno] = count
        else
          lineno_to_count[lineno].merge(count)
        end
      end
    end

    summary = CoverageSummary.new()
    handle = source_file.sub(/\.gcov$/, "")
    summaries[handle] = summary

    lineno_to_count.size.times do |lineno|
      count = lineno_to_count[lineno]
      summary.merge(count)
      output_file.puts count.to_s
    end
    output_file.close()

  end

  summaries.sort_by do |_, summary|
    summary.fraction_covered
  end.each do |handle, summary|
    summary.summarize(options, STDOUT, handle)
  end

  if options.verbose
    puts "\n"
  end

  summaries.sort_by do |handle, _|
    handle
  end.each do |handle, summary|
    summary.summarize(options, STDOUT, handle)
  end

  if options.verbose
    puts "\n"
  end

  all = CoverageSummary.new
  summaries.each do |_, summary|
    all.notexecuted += summary.notexecuted
    all.executed += summary.executed
  end

  all.summarize(options, STDOUT, "all")
end

options = OpenStruct.new
options.output_dir_name = "gcov_aggregate"
options.verbose = false

opts = OptionParser.new do |opts|
  opts.banner = "Usage: merge_gcov [options] gcov_stash_directory_names"

  opts.on("-o", "--output_dir [OUTPUT_DIR_NAME]", "Output Directory") do |dirname|
    options.output_dir_name = dirname
  end

  opts.on("-v", "--verbose", "Be verbose") do
    options.verboes = true
  end

end
other_args = opts.parse!(ARGV)

aggregate_many_gcov_files(options, other_args)
