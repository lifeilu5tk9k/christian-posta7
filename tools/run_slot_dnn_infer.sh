
#rm -rf performance.txt
rm -rf slot_dnn_infer.log

trainingFile=/ssd3/wangbin44/PaddleRec/tools/inference_c++2.0/data/out_test.1
modelFile=/ssd3/wangbin44/PaddleRec/tools/inference_c++2.0/data/rec_inference/full_inference.pdmodel
paramFile=/ssd3/wangbin44/PaddleRec/tools/inference_c++2.0/data/rec_inference/full_inference.pdiparams
performanceFile=/ssd3/wangbin44/PaddleRec/tools/performance.txt
iteration_num=10
test_predictor=True

#for threadNum in 1 2 4 8 16 24 32 64
#do
#    for batchSize in 1
#    do
#        echo "++++ executing task : threadNum - $threadNum, batchSize - $batchSize"
#        python3 -u slot_dnn_infer_dataloader.py --thread_num $threadNum --batchsize $batchSize --iteration_num $iteration_num --reader_file $trainingFile --model_file $modelFile --params_file $paramFile --performance_file $performanceFile --test_predictor $test_predictor
#    done
#done

for threadNum in 1 16
do
    for batchSize in 32
    do
        echo "++++ executing task : threadNum - $threadNum, batchSize - $batchSize"
        python3 -u slot_dnn_infer_dataloader.py --thread_num $threadNum --batchsize $batchSize --iteration_num $iteration_num --reader_file $trainingFile --model_file $modelFile --params_file $paramFile --performance_file $performanceFile --test_predictor $test_predictor
    done
done

#for threadNum in 64
#do
#    for batchSize in 1
#    do
#        echo "++++ executing task : threadNum - $threadNum, batchSize - $batchSize"
#        python3 -u slot_dnn_infer_dataloader.py --thread_num $threadNum --batchsize $batchSize --iteration_num $iteration_num --reader_file $trainingFile --model_file $modelFile --params_file $paramFile --performance_file $performanceFile --test_predictor $test_predictor
#    done
#done
