#!/bin/bash

for f in $1/*.gz; do
	if [[ $f != *"tar.gz"* ]]
	then
		$2 $f ${f}.simple_l1i.csv
	fi
done