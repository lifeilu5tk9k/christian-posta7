#!/bin/bash
echo "................run................."
python -m paddlerec.run -m ./config.yaml &>result1.txt
grep -i "prediction" ./result1.txt >./result.txt
rm -f result1.txt
python eval.py
