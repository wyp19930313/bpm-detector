//
// Created by Luke Mino-Altherr on 2/23/16.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "kiss_fft.h"
#include "bpm_detector.h"
#define TRUE 1
#define FALSE 0

unsigned char buffer4[4];
unsigned char buffer2[2];

//char* seconds_to_time(float seconds);

FILE *ptr;

int main() {
  // Get file path.
  char * filename = NULL;
  filename = (char *) malloc(1024 * sizeof(char));

  if (getcwd(filename, 1024) != NULL) {
    strcat(filename, "/");
    strcat(filename, "140.wav");
  }

  // open file
  printf("Opening file %s...\n", filename);
  ptr = fopen(filename, "r");
  if (ptr == NULL) {
    printf("Error opening file\n");
    exit(1);
  }

  int read = 0;
  WAVE * wave;
  wave = (WAVE *) malloc(sizeof(WAVE));

  // Read in WAVE attributes
  read = fread(wave->riff, sizeof(wave->riff), 1, ptr);
  printf("(1-4): %s \n", wave->riff);

  read = fread(buffer4, sizeof(buffer4), 1, ptr);
  //printf("%u %u %u %u\n", buffer4[0], buffer4[1], buffer4[2], buffer4[3]);

  // convert little endian to big endian 4 byte int
  wave->overall_size = buffer4[0] |
                       (buffer4[1]<<8) |
                       (buffer4[2]<<16) |
                       (buffer4[3]<<24);

  printf("(5-8) Overall size: bytes:%u, Kb:%u \n", wave->overall_size, wave->overall_size/1024);

  read = fread(wave->wave, sizeof(wave->wave), 1, ptr);
  printf("(9-12) Wave marker: %s\n", wave->wave);

  read = fread(wave->fmt_chunk_marker, sizeof(wave->fmt_chunk_marker), 1, ptr);
  printf("(13-16) Fmt marker: %s\n", wave->fmt_chunk_marker);

  read = fread(buffer4, sizeof(buffer4), 1, ptr);
  //printf("%u %u %u %u\n", buffer4[0], buffer4[1], buffer4[2], buffer4[3]);

  // convert little endian to big endian 4 byte integer
  wave->length_of_fmt = buffer4[0] |
                        (buffer4[1] << 8) |
                        (buffer4[2] << 16) |
                        (buffer4[3] << 24);
  printf("(17-20) Length of Fmt wave: %u \n", wave->length_of_fmt);

  read = fread(buffer2, sizeof(buffer2), 1, ptr);
  //printf("%u %u \n", buffer2[0], buffer2[1]);

  wave->format_type = buffer2[0] | (buffer2[1] << 8);
  char format_name[10] = "";
  if (wave->format_type == 1)
    strcpy(format_name,"PCM");
  else if (wave->format_type == 6)
    strcpy(format_name, "A-law");
  else if (wave->format_type == 7)
    strcpy(format_name, "Mu-law");

  printf("(21-22) Format type: %u %s \n", wave->format_type, format_name);

  read = fread(buffer2, sizeof(buffer2), 1, ptr);
  //printf("%u %u \n", buffer2[0], buffer2[1]);

  wave->channels = buffer2[0] | (buffer2[1] << 8);
  printf("(23-24) Channels: %u \n", wave->channels);
  if (wave->channels != 2) {
    printf("Assumed there would be two channels. Exiting.");
    exit(1);
  }

  read = fread(buffer4, sizeof(buffer4), 1, ptr);
  //printf("%u %u %u %u\n", buffer4[0], buffer4[1], buffer4[2], buffer4[3]);

  wave->sample_rate = buffer4[0] |
                      (buffer4[1] << 8) |
                      (buffer4[2] << 16) |
                      (buffer4[3] << 24);

  printf("(25-28) Sample rate: %u\n", wave->sample_rate);

  read = fread(buffer4, sizeof(buffer4), 1, ptr);
  //printf("%u %u %u %u\n", buffer4[0], buffer4[1], buffer4[2], buffer4[3]);

  wave->byterate  = buffer4[0] |
                    (buffer4[1] << 8) |
                    (buffer4[2] << 16) |
                    (buffer4[3] << 24);
  printf("(29-32) Byte Rate: %u , Bit Rate:%u\n", wave->byterate, wave->byterate*8);

  read = fread(buffer2, sizeof(buffer2), 1, ptr);
  //printf("%u %u \n", buffer2[0], buffer2[1]);

  wave->block_align = buffer2[0] |
                      (buffer2[1] << 8);
  printf("(33-34) Block Alignment: %u \n", wave->block_align);

  read = fread(buffer2, sizeof(buffer2), 1, ptr);
  //printf("%u %u \n", buffer2[0], buffer2[1]);

  wave->bits_per_sample = buffer2[0] |
                          (buffer2[1] << 8);
  printf("(35-36) Bits per sample: %u \n", wave->bits_per_sample);

  read = fread(wave->data_chunk_header, sizeof(wave->data_chunk_header), 1, ptr);
  printf("(37-40) Data Marker: %s \n", wave->data_chunk_header);

  read = fread(buffer4, sizeof(buffer4), 1, ptr);
  //printf("%u %u %u %u\n", buffer4[0], buffer4[1], buffer4[2], buffer4[3]);

  wave->data_size = buffer4[0] |
                    (buffer4[1] << 8) |
                    (buffer4[2] << 16) |
                    (buffer4[3] << 24 );
  printf("(41-44) Size of data chunk: %u \n", wave->data_size);

  long size_of_each_sample = (wave->channels * wave->bits_per_sample) / 8;
  printf("Size of each sample:%ld bytes\n", size_of_each_sample);

  long num_samples = (wave->data_size) / (size_of_each_sample);
  printf("Number of samples:%lu \n", num_samples);

  // calculate duration of file
  float duration_in_seconds = (float) wave->overall_size / wave->byterate;
  printf("Approx.Duration in seconds=%f\n", duration_in_seconds);
  //printf("Approx.Duration in h:m:s=%s\n", seconds_to_time(duration_in_seconds));

  // read each sample from data chunk if PCM
  if (wave->format_type == 1) { // PCM

    int  size_is_correct = TRUE;

    // make sure that the bytes-per-sample is completely divisible by num.of channels
    long bytes_in_each_channel = (size_of_each_sample / wave->channels);
    if ((bytes_in_each_channel  * wave->channels) != size_of_each_sample) {
      printf("Error: %ld x %ud <> %ld\n", bytes_in_each_channel, wave->channels, size_of_each_sample);
      size_is_correct = FALSE;
    }

    if (size_is_correct) {
      // the valid amplitude range for values based on the bits per sample
      long low_limit = 0l;
      long high_limit = 0l;

      switch (wave->bits_per_sample) {
        case 8:
          low_limit = -128;
              high_limit = 127;
              break;
        case 16:
          low_limit = -32768;
              high_limit = 32767;
              break;
        case 32:
          low_limit = -2147483648;
              high_limit = 2147483647;
              break;
      }


      printf("\n.Valid range for data values : %ld to %ld \n", low_limit, high_limit);

      /***********************************************************************************/
      // Start computing bpm
      /***********************************************************************************/

      long i = 0;
      int j = 0, k = 0, cnt = 0, bpm = 0, prev_bpm = 0;
      char * data_buffer;
      data_buffer = (char *) calloc(size_of_each_sample/2, sizeof(char));

      int * frequency_map;
      frequency_map = (int *) calloc(200, sizeof(int));

      int step_size = 1024;
      int sub_band_size = 32;
      int num_steps = num_samples / step_size;
      float temp_data;

      float *data, *abs_buffer, *E_subband;
      data = (float *) calloc(2, sizeof(float));
      abs_buffer = (float *) calloc(step_size, sizeof(float));
      E_subband = (float *) calloc(sub_band_size, sizeof(float));

      int current_bpm=0, average_bpm=0;
      float current_peak=0.0, previous_peak=0.0, front_peak=0.0, end_peak=0.0, transient_peak=0.0;
      int start_peak=FALSE;

      // Allocate (sub_band_size) queues to hold 43 history energy values
      Queue * energy_history;
      int capacity = 80;
      energy_history = (Queue *) malloc(sub_band_size * sizeof(Queue));
      for (i = 0; i < sub_band_size; i++) {
        energy_history[i].capacity = capacity;
        energy_history[i].size = 0;
        energy_history[i].front = 0;
        energy_history[i].rear = -1;
        energy_history[i].data = (float *) calloc(capacity, sizeof(float));
      }

      // Allocate FFT buffers
      kiss_fft_cpx *fft_input_ch1, *fft_input_ch2, *fft_output_ch1, *fft_output_ch2;
      fft_input_ch1 = (kiss_fft_cpx *) malloc(step_size * sizeof(kiss_fft_cpx));
      fft_input_ch2 = (kiss_fft_cpx *) malloc(step_size * sizeof(kiss_fft_cpx));
      fft_output_ch1 = (kiss_fft_cpx *) malloc(step_size * sizeof(kiss_fft_cpx));
      fft_output_ch2 = (kiss_fft_cpx *) malloc(step_size * sizeof(kiss_fft_cpx));

      for (i = 0; i < step_size; i++){
        fft_input_ch1[i].r = 0;
        fft_input_ch1[i].i = 0;
        fft_output_ch1[i].r = 0;
        fft_output_ch1[i].i = 0;
      }

      kiss_fft_cfg fft_cfg_ch1 = kiss_fft_alloc(step_size, 0, NULL, NULL);
      kiss_fft_cfg fft_cfg_ch2 = kiss_fft_alloc(step_size, 0, NULL, NULL);

      for (i = 0; i < num_steps; i++) {

        for (j = 0; j < step_size; j++) {

          for (k=0; k < 2; k++) {
            read = fread(data_buffer, sizeof(data_buffer), 1, ptr);

            switch (bytes_in_each_channel) {
              case 4:
                data[k] = data_buffer[0] |
                          (data_buffer[1] << 8) |
                          (data_buffer[2] << 16) |
                          (data_buffer[3] << 24);
                    break;
              case 2:
                data[k] = data_buffer[0] |
                          (data_buffer[1] << 8);
                    break;
              case 1:
                data[k] = data_buffer[0];
                    break;
            }

            // check if value was in range
            if (data[k] < low_limit || data[k] > high_limit)
              printf("**value out of range\n");
          }

          //printf("Data L: %f, Data R: %f\n", data[0], data[1]);

          // Initialize buffers
          fft_input_ch1[j].r = (kiss_fft_scalar) data[0];
          fft_input_ch1[j].i = (kiss_fft_scalar) data[1];
          //fft_input_ch2[j].r = (kiss_fft_scalar) data[1];
          //fft_input_ch2[j].i = 0.0;
          fft_output_ch1[j].r = 0.0;
          fft_output_ch1[j].i = 0.0;
          fft_output_ch2[j].r = 0.0;
          fft_output_ch2[j].i = 0.0;

          // Compute fft
          kiss_fft(fft_cfg_ch1, fft_input_ch1, fft_output_ch1);
          //kiss_fft(fft_cfg_ch2, fft_input_ch2, fft_output_ch2);

          //printf("FFT R: %f, FFT I: %f\n", fft_output_ch1[j].r, fft_output_ch1[j].i);

          // Compute abs of FFTs
          abs_buffer[j] = pow(fft_output_ch1[j].r, 2) + pow(fft_output_ch1[j].i, 2);
          //abs_buffer[j] += pow(fft_output_ch2[j].r, 2) + pow(fft_output_ch2[j].i, 2);
          //printf("Abs: %f\n", abs_buffer[j]);
        }



        // Compute subband energys
        for (j = 0; j < sub_band_size; j++) {
          temp_data = 0;
          for (k = j*sub_band_size; k < (j+1)*sub_band_size; k++) {
            temp_data += abs_buffer[k];
            //printf("Tmp: %f\n", temp_data);
          }

          E_subband[j] = temp_data*sub_band_size/step_size;
          //printf("E: %f\n", E_subband[j]);

          //printf("Subband: %f, Average: %f\n", E_subband[j], average_queue(energy_history[j]));

          // Energy peak detected
          if (E_subband[j] > 250*average_queue(energy_history[j])){
            printf("!\n\n");
            if (previous_peak != current_peak)
              previous_peak = current_peak;
            current_peak = (float)(i*step_size+j*sub_band_size)/wave->sample_rate;
            //current_peak = floorf(current_peak*100)/100;
            //printf("Beat at second %f\n", current_peak);

            // Begin tracking of a transient peak.
            // Take average of front and back when peak end detected.
            if ((current_peak - previous_peak) > .3 && start_peak == FALSE) {
              front_peak = current_peak;
              start_peak = TRUE;
            } else if ((current_peak - previous_peak) > .3 && start_peak == TRUE && previous_peak != 0){
              end_peak = previous_peak;
              transient_peak = (front_peak + end_peak)/2;
              printf("%f\n", transient_peak);
              start_peak = FALSE;
            }

            /*if (previous_beat != 0) {
              current_bpm = 60*(current_peak - previous_peak);
              if (40 < current_bpm && current_bpm < 200)
                frequency_map[current_bpm] += 1;
              //average_bpm = (average_bpm + current_bpm) / 2;
            }*/
            //printf("Current bpm %i\n", current_bpm);
            //break;
          }

          if (energy_history[j].size == energy_history[j].capacity)
            dequeue(energy_history, j);
          enqueue(energy_history, j, E_subband[j]);
        }
        /*bpm = most_frequent_bpm(frequency_map);
        if (bpm != 0 && bpm != prev_bpm)
          printf("Current bpm is %i.\n", most_frequent_bpm(frequency_map));
        prev_bpm = bpm;*/

      } // for (i=0; i<num_steps; i++) {

      //dump_map(frequency_map);

      free(energy_history);
      free(data);
      free(abs_buffer);
      free(E_subband);
      free(fft_input_ch1);
      free(fft_output_ch1);
      free(fft_input_ch2);
      free(fft_output_ch2);

    } // if (size_is_correct) {
  } //  if (wave->format_type == 1) {

  if (ptr) {
    printf("Closing file..\n");
    fclose(ptr);
    ptr = NULL;
  }

  free(wave);
  free(filename);

}

void dequeue(Queue * q, int index) {
  if (q[index].size == 0){
    printf("Can't dequeue. Queue is empty.");
    return;
  } else {
    q[index].front = q[index].front + 1;
    q[index].size = q[index].size + 1;
    if (q[index].front == q[index].capacity)
      q[index].front = 0;
  }
  return;
}

void enqueue(Queue * q, int index, float data) {

  if (q[index].size == q[index].capacity) {
    printf("Cant enqueue. Queue is full.");
    return;
  } else {
    q[index].size = q[index].size + 1;
    q[index].rear = q[index].rear + 1;
    if(q[index].rear == q[index].capacity)
      q[index].rear = 0;
    q[index].data[q[index].rear] = data;
  }
  return;
}

float average_queue(Queue q) {
  int i = q.front;
  float average = 0.0;
  while (1) {
    average += q.data[i];
    if (i == q.rear)
      break;
    i++;
    if (i == q.capacity)
      i=0;
  }
  return average/q.size;
}

int most_frequent_bpm(int * map) {
  int i, winner=0;
  for (i=1; i<200; i++) {
    if (map[i] > winner)
      winner = i;
  }
  return winner;
}

void dump_map(int * map) {
  int i;
  for (i = 0; i < 200; i++)
    printf("%i, %i\n", i, map[i]);
  return;
}