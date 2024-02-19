#!/bin/sh

echo "Content-TYPE: text/html

<html><body>"

####################################

if [ $# -gt 0 ]; then
   # There are command line arguments

   echo "<p>I have received the parameters as command line arguments</p>
   <pre>Number of arguments: $#"

   for i in $*; do
      echo "Argument: $i"
   done

   echo "</pre>"

####################################

elif [ "$QUERY_STRING" != "" ]; then
   # The environment variable QUERY_STRING is set

   echo "<p>I have received the parameters through the QUERY_STRING environment variable</p>
   <pre>Variable contents: $QUERY_STRING</pre>"

####################################

else
   # Display the standard input

   echo "<p>I have received the parameters as standard input</p>
   <pre>"
   cat
   echo "</pre>"
fi

echo "</body></html>"

