# logtrunc
Truncate timestamped logfiles leaving the most recent entries

This tool can trim logfiles leaving only those entries after a certain
timestamp. A requirement for that is that the logfiles are timestamped in an
incremental-sorting manner, for instance 'yyyy-mm-dd hh:mm:ss'. Logtrunc offers
facilities to work with daemons that may need to be reloaded before they start
using a newly created logfile. It can also linger for a while to catch
subprocesses that may continue to write to the previous (unlinked) logfile for
a while. Unfortunately such tricks are necessary because truncating the start
of a file rather than the end is an unnatural operation for POSIX-like
filesystems. In the future COLLAPSE_RANGE type of operations may improve this
situation, but in all likelihood these functions will be limited to specific
alignments so may not work for a logfile with random contents.

## Usage
    logtrunc [-i] [-w waittime] [-e subcommand] timestamp filename

    -i    Ignore lines not starting with the pattern matching the timestamp.
          The pattern is constructed by matching each character in the
          timestamp to a character class (one of alphabetical, numerical or
          punctuation). Characters that don't fall into one of these classes
          are put into the pattern literally. Lines in the source file are then
          compared to the pattern and ignored if they don't match.
          Ignore in this context means to skip these lines until the
          requested timestamp is passed and copy them over afterwards.

    -w waittime
          Monitor the old logfile for <waittime> seconds and copy over any
          lines that are still written to it by lingering processes to the
          new logfile.

    -e subcommand
          Execute <subcommand> once the new logfile is in place. This can be
          used to signal daemon processes to reopen their logfiles.
