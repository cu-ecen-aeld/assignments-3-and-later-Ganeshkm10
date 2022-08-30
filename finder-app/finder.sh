#! /bin/bash
filesdir=$1
searchstr=$2

#Check for number of arguments
if [ $# -ne 2 ]
then
   echo 'Invalid Number of Arguments'
   echo 'provide 2 arguments'
   echo ': the first argument is a path to a directory on the filesystem'
   echo ': second argument is a text string which will be searched within these files'
   exit 1	
fi
#check the first argument passed is a directory or not
if [ ! -d $filesdir ]
then
   echo " First argument ${filesdir} passed is not a directory"
   exit 1
fi

# gets the number of files in directory and its sub directories
X=$( find $filesdir -type f | wc -l)
# gets the number of matching string
Y=$( grep -r $searchstr $filesdir | wc -l)
# print message
echo " The number of files are ${X} and the number of matching lines are ${Y} "
