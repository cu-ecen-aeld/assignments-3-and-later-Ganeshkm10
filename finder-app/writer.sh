#! /bin/bash

writefile=$1
writestr=$2

#Check for number of arguments
if [ $# -ne 2 ]
then
   echo 'Invalid Number of Arguments'
   echo 'provide 2 arguments'
   echo ': the first argument is a path to a directory on the filesystem'
   echo ': second argument is a text string which will be searched within these files'
   exit 1	
fi

mkdir -p "$(dirname "$writefile")" && touch "$writefile"
echo "$writestr" > "$writefile"

if [[ ! -f $writefile ]]
then
    echo "$writefile file could not be created."
    exit 1
else
	echo "$writefile has been updated"
fi
