#Raw Traces

All the raw traces that were generated for the thesis can be found at: https://drive.google.com/drive/folders/0B_mmdcSttS0eaGNhWDlZQzU3bGs?usp=sharing

This Google Drive Folder includes traces for TPC-H queries 1, 3, 6, 14, and 19 on MySQL in its mysql subdirectory, Hive in any instrace_* subdirectory, and some spec traces in its spec subdirectory. All traces are gzipped.

Included with the traces are the raw outputs from several scripts that analyzed the traces or ran the traces through an L1i simulator. The runahead-prefetch simulator is one obvious program that used the traces, and those files have the word "complex" in them. Additionally, simple_l1i and uarch_indep_metrics scripts were also used on the traces. Those scripts are included in subdirectories here, with their own READMEs.