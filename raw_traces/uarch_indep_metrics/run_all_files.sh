#!/bin/bash

for f in $1/*.gz; do
	if [[ $f != *"tar.gz"* ]]
	then
		$2 $f ${f}.uarch_indep_metrics.csv
	fi
done