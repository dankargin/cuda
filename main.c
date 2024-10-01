#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <chrono>
#include <iostream>

//#ifndef __CUDACC__ 
//#define __CUDACC__
//#endif
//#include <device_functions.h>

__global__ void countSort(int* arr, int* output, int radix, int n, int offset, int* count) {

    __shared__ int shared_count[10];
    for (int i = 0; i < 10; i++) shared_count[i] = count[i];

    int tid = threadIdx.x; // номер потока в рамках одного блока, ranges from 0 to blockDim.x-1
    int tid_all = blockIdx.x * blockDim.x + threadIdx.x; // номер потока для управления в рамках всей сетки, ranges from 0 to gridDim.x-1

    int range = 0; // на случай, если кол-во элементов не кратно кол-ву элементов в блоке
    if ((blockIdx.x > n / blockDim.x) && (offset != 0))      // если блок последний и кол-во элементо в в массиве не ратно кол-ву потоков в блоке
        range = blockIdx.x * blockDim.x + range;
    else
        range = blockIdx.x * blockDim.x + blockDim.x;

    //for (int i = blockDim.x * blockIdx.x; i < range; i++) // подсчет
    if ((blockIdx.x > n / blockDim.x) && (offset != 0)) {
        if (tid < offset) {
            shared_count[(arr[tid] / radix) % 10]++;
        }
    }
    else shared_count[(arr[tid] / radix) % 10]++;
        
    //for (int i = 0; i<n; i++)
    //    if (((arr[i] / radix) % 10) == tid)
    //         count[tid]++;

    //__syncthreads();

    if (tid == 0) for (int i = 0; i < 10; i++) count[i] += shared_count[i];

    //if (tid == 0) for (int a = 0; a < 10; a++) if (a != 0) count[a] += count[a - 1];

    //for (int i = n - 1; i >= 0; i--) {
    //    output[count[(arr[i] / radix) % 10] - 1] = arr[i];
    //    count[(arr[i] / radix) % 10]--;
    //}

    //for (int i = n - 1; i >= 0; i--) {
    //    if (((arr[i] / radix) % 10) == tid) {
    //
    //        output[count[tid] - 1] = arr[i];
    //
    //        count[tid]--;
    //    }
    //}
    __syncthreads();
    return;

}

__global__ void count1(int* arr, int* output, int radix, int n, int offset, int* count) {

    int tid = threadIdx.x;
    if (tid == 0) for (int a = 0; a < 10; a++) if (a != 0) count[a] += count[a - 1];
    for (int i = n - 1; i >= 0; i--) {
        if (((arr[i] / radix) % 10) == tid) {
    
            output[count[tid] - 1] = arr[i];
    
            count[tid]--;
        }
    }
}

void radixSort(int* arr, int n) {

    cudaError_t CudaErrorStatus = cudaSetDevice(0);
    if (CudaErrorStatus != cudaSuccess)
    {
        fprintf(stderr, "cudaSetDevice failed! Do you have a CUDA-capable GPU installed?");
    }

    int* d_arr;
    int* d_output;
    int m = arr[0];
    for (int i = 1; i < n; i++) // считаем самый большой элемент в массиве
        if (arr[i] > m)
            m = arr[i];
    int array_scale = n * sizeof(int);

    CudaErrorStatus = cudaMalloc((void**)&d_arr, array_scale);
    if (CudaErrorStatus != cudaSuccess)
    {
        fprintf(stderr, "cudaMalloc failed!");
        return;
    }
    CudaErrorStatus = cudaMemcpy(d_arr, arr, array_scale, cudaMemcpyHostToDevice);
    if (CudaErrorStatus != cudaSuccess)
    {
        fprintf(stderr, "cudaMemcpy failed!");
        return;
    }
    CudaErrorStatus = cudaMalloc((void**)&d_output, array_scale);
    if (CudaErrorStatus != cudaSuccess)
    {
        fprintf(stderr, "cudaMalloc failed!");
        return;
    }
    auto bg = std::chrono::steady_clock::now();
    //double start = clock();
    printf("\n starting");
    int count[10];
    for (int i = 0; i < 10; i++) count[i] = 0;

    int* d_count;
    CudaErrorStatus = cudaMalloc((void**)&d_count, 10 * sizeof(int));
    if (CudaErrorStatus != cudaSuccess) {
        fprintf(stderr, "cudaMalloc failed!");
        return;
    }
    CudaErrorStatus = cudaMemcpy(d_count, count, 10 * sizeof(int), cudaMemcpyHostToDevice);
    if (CudaErrorStatus != cudaSuccess) {
        fprintf(stderr, "123 cudaMemcpy failed!");
        return;
    }

    for (int radix = 1; m / radix > 0; radix *= 10) {

        int num_blocks = 0;
        int offset = 0;
        
        if (n < 1024) num_blocks = 1; // считаем кол-во блоков, так, чтобы один элемент массива - 1 поток, учитывая, что максимум потоков в блоке 1024
        else {
            num_blocks = n / 1024;
            if (num_blocks * 1024 < n) n++;
            offset = n - (num_blocks * 1024);
        }
        
        countSort <<<num_blocks, 1024 >>> (d_arr, d_output, radix, n, offset, d_count);
        
        count1 << <1, 10 >> > (d_arr, d_output, radix, n, offset, d_count);
        //countSort <<<1, 10 >>> (d_arr, d_output, radix, n);

        int* temp = d_arr;
        d_arr = d_output;
        d_output = temp;
    }

    //double end = clock();
    //double time = end - start;
    //printf("\n time: %d", time);

    //CudaErrorStatus = cudaDeviceSynchronize();
    //if (CudaErrorStatus != cudaSuccess)
    //{
    //    fprintf(stderr, "cudaDeviceSynchronize returned error code %d after launching countSort!\n", CudaErrorStatus);
    //    return;
    //}

    printf("\nCopying back to the initial array...\n");

    CudaErrorStatus = cudaMemcpy(arr, d_arr, array_scale, cudaMemcpyDeviceToHost);
    if (CudaErrorStatus != cudaSuccess)
    {
        fprintf(stderr, "cudaMemcpy failed!");
        return;
    }

    auto en = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(en - bg);
    std::cout << "\n\nThe time: " << elapsed_ms.count() << " ms\n";

    cudaFree(d_arr);
    cudaFree(d_output);
}

int main() {

    cudaError_t CudaErrorStatus;

    int array_length = 100;
    //array_length *= array_length;

    int* array_to_sort = NULL;

    array_to_sort = (int*)malloc(sizeof(int) * array_length);
    if (array_to_sort == NULL) {
        printf("\n error\n");
        return 0;
    }

    srand(time(NULL));

    printf("\n creating an array...");
    for (int i = 0; i < array_length; i++) {

        *(array_to_sort + i) = rand() % 1000;
    }

    //printf("\narray: \n");
    //for (int i = 0; i < array_length; i++)
    //    printf("%4d ", array_to_sort[i]);
    //printf("\n");

    printf("\n call radixSort...");

    radixSort(array_to_sort, array_length);

    //printf("\n checking if the array is correct...");
    //for (int i = 0; i < array_length-1; i++) if (array_to_sort[i] > array_to_sort[i + 1]) { printf("sort error"); return 1; }
    //printf("\n array is correct, exiting...");

    printf("Sorted array: \n");
    for (int i = 0; i < array_length; i++)
        printf("%4d ", array_to_sort[i]);
     printf("\n");

    CudaErrorStatus = cudaDeviceReset();
    if (CudaErrorStatus != cudaSuccess)
        fprintf(stderr, "cudaDeviceReset failed!");


    return 0;
}
