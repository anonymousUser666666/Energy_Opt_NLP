#!/usr/bin/bash

title="MIGP"
MaxTaskNumber=20

# clear buffer file content
time_file="time_task_number.txt"
> $time_file

cp parameters.yaml ../../sources/parameters.yaml

cd ../..
mkdir release
cd release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4



for jobNumber in {5..20}
do
	cd ../CompareWithBaseline/$title
	echo "$title iteration is: $jobNumber"
	> EnergySaveRatio/N$jobNumber.txt
	# generate task set

  	python ../ConvertYechengDataset.py --convertionNumber $jobNumber

	# Optimize energy consumption
	cd ../../release

	make -j4
	./tests/LLCompare1
	#./tests/WAPBatchCompare
	cd ../CompareWithBaseline
	sleep 1
done

cd Zhao20
# visualize the result
python Visualize_distribution.py --minTaskNumber 5 --maxTaskNumber $MaxTaskNumber --baseline "MUA-incremental" 
python Visualize_average_speed.py --minTaskNumber 5 --baseline "$title" --ylim 1e2 --maxTaskNumber $MaxTaskNumber
