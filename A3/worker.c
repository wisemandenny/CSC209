#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "freq_list.h"
#include "worker.h"

#define FILENAMES_STR_LENGTH 10
#define INDEX_STR_LENGTH 6
#define CONVERTED_STR_LENGTH 21

int num_of_files = -1;

/* fopen with error checking */
FILE *Fopen(char *file_name, char *mode) {
  FILE *ret = fopen(file_name, mode);
  if (ret == NULL) {
    perror("Fopen");
    exit(1);
  }
  return ret;
}

/* close with error checking */
int Close(int fd) {
  int ret = close(fd);
  if (ret == -1) {
    perror("close failed");
    exit(1);
  }
  return ret;
}

/* read with error checking */
int Read (int fd, void *buf, size_t nbytes) {
  int ret = read(fd, buf, nbytes);
  if (ret == -1) {
    perror("read failed");
    exit(1);
  }
  return ret;
}

/* write with error checking */
int Write(int fd, const void *buf, size_t nbytes) {
  int ret = write(fd, buf, nbytes);
  if (ret < 0) {
    perror("write failed");
    exit(1);
  }
  return ret;
}

/* malloc with error checking */
void *Malloc (size_t size) {
  void *ret = malloc(size);
  if (ret == NULL) {
    perror("malloc failed");
    exit(1);
  }
  return ret;
}

/* Returns an empty FreqRecord struct */
FreqRecord *get_empty_freqrecord () {
  FreqRecord *empty_freq = Malloc(sizeof(FreqRecord));
  empty_freq->freq = 0;
  strcpy(empty_freq->filename, "\0");
  return empty_freq;
}

/* Returns the number of lines in a file */
int count_lines (FILE *fp) {
  int lines = 0;
  char line[MAXLINE];

  while(fgets(line, MAXLINE, fp) != NULL) {
    lines++;
  }
  return lines;
}

/*
* Converts a binary file to a text file
* UNCOMMENT THIS IF YOU WANT TO TRY BINARY TO TEXT CONVERSION

void binary_to_text(char *input) {
  char output[strlen(input)];
  strcpy(output, input);
  output[(strlen(input) -3)] = 't';
  output[strlen(input) -2] = 'x';
  output[strlen(input) -1] = 't';
  unsigned char line[256];
  unsigned int i;
  int j, len;

  FILE *input_fp, *output_fp;

  input_fp = Fopen(input, "rb");
  output_fp = Fopen(output, "w");

  while ((len = fgetc(input_fp)) != EOF) {
    fread(line, len, 1, input_fp);
    line[len] = '\0';
    i = fgetc(input_fp) << 24;
    i |= fgetc(input_fp) << 16;
    i |= fgetc(input_fp) << 8;
    i |= fgetc(input_fp);
    fprintf(output_fp, "%s %d\n", (char*)line, i);
  }
  if ((j = fclose(input_fp)) != 0) {
    perror("fclose on binary_to_text input_fp");
    exit(1);
  }

  if ((j = fclose(output_fp)) != 0) {
    perror("fclose on binary_to_text output_fp");
    exit(1);
  }

  return;
}*/

/*
* Returns an array of FreqRecord structs for the word word.
* One FreqRecord for each file where the word is found
*/
FreqRecord *get_word(char *word, Node *head, char **file_names) {
  // figure out how many file names there are only if this has never been calculated before
  if (num_of_files == -1){
    num_of_files = 0;
    while(strlen(file_names[num_of_files]) > 0) {
      num_of_files++;
    }
  }
  // for(number_of_files = 0; file_names[number_of_files] != NULL; number_of_files++) {}
  // declare an array of structs the same length as the number of file file_names
  FreqRecord *frequencies = Malloc(num_of_files * sizeof(FreqRecord));
  // iterate through the linked list
  Node *cur = head;

  int num_found = 0;
  while(cur) {
    if (strcmp(cur->word, word) == 0) { // if the word is found in the index
      for (int i = 0; i < num_of_files; i++) {
        // if you find the word, copy the frequency at that position to the FreqRecord
        frequencies[i].freq = cur->freq[i];
        strncpy(frequencies[i].filename, file_names[i], PATHLENGTH);
        num_found++;
      }
    }
    if(cur->next) { //advance to the next item in the linkedlist
      cur = cur->next;
    } else { // no more items, add the empty frequency to show no more data
      frequencies[num_found] = *get_empty_freqrecord();
      cur = NULL;
    }
  }
  return frequencies;
}

/* Print to standard output the frequency records for a word.
* Use this for your own testing and also for query.c
*/
void print_freq_records(FreqRecord *frp) {
    int i = 0;
    while (strlen(frp[i].filename) > 0) {
        printf("%d\t%s\n", frp[i].freq, frp[i].filename);
        i++;
    }
}

/* Takes the path to a directory that
* contains the index and filenames files, as well as the file descriptors
* representing the read end (in) and the write end (out) of the pipe that
* connects it to the parent.
* It writes to the out file descriptor one FreqRecord for each file in which the
* word has a non-zero frequency, followed by an empty FreqRecord to signal the
* end of data.
*/
void run_worker(char *dirname, int in, int out) {
  Node *head = NULL;
  FILE *filenames_fp; // *output_binary_fp;
  // int converted_output_fd;
  FreqRecord *frequencies;
  char query_word[MAXWORD];
  char index_file_path[strlen(dirname) + INDEX_STR_LENGTH + 1];
  char filenames_file_path[strlen(dirname) + FILENAMES_STR_LENGTH + 1];
  // char converted_output_file_path[strlen(dirname) + CONVERTED_STR_LENGTH + 1];

  strcpy(filenames_file_path, dirname);
  filenames_fp = Fopen(strcat(filenames_file_path, "/filenames"), "r");

  num_of_files = count_lines(filenames_fp);
  if (fclose(filenames_fp) != 0) {
    perror("fclose for names file");
    exit(1);
  }

  char *filenames[num_of_files];

  strcpy(index_file_path, dirname);
  strcat(index_file_path, "/index");
  read_list(index_file_path, filenames_file_path, &head, filenames);

  // strcpy(converted_output_file_path, dirname);
  // strcat(converted_output_file_path, "/converted_output.bin");

  filenames[num_of_files] = "\0";
  while(Read(in, query_word, MAXWORD) > 0) {
    query_word[strlen(query_word) - 1] = '\0'; //remove \n character
    frequencies = get_word(query_word, head, filenames);
    for (int i = 0; i < num_of_files; i++) {
      if(frequencies[i].freq > 0) {
        // printf("%d\t%s\n", frequencies[i].freq, frequencies[i].filename);            UNCOMMENT THIS IF YOU WANT TO TRY BINARY -> TEXT CONVERSION
        // output_binary_fp = Fopen(converted_output_file_path, "ab");
        // if((converted_output_fd = fileno(output_binary_fp)) == -1) {
        //   perror("fileno for converted output file");
        //   exit(1);
        // }
        // dup2(converted_output_fd, out);
        Write(out, &(frequencies[i]), sizeof(FreqRecord));
      }
    }
  }
  Close(in);
  Write(out, get_empty_freqrecord(), sizeof(FreqRecord));
  // binary_to_text(converted_output_file_path);                                      THIS TOO
  Close(out);
  return;
}
