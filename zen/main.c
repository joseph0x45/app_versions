#include "./cJSON.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ZEN_RELEASES "https://api.github.com/repos/zen-browser/desktop/releases"
#define DOWNLOAD_BASE_URL                                                      \
  "https://github.com/zen-browser/desktop/releases/download/"
#define FILE_NAME "zen-x86_64.AppImage"
#define DOWNLOAD_FOLDER "/home/joseph/Downloads/"

void print_usage(void) {
  fprintf(stdout, "Zen updater (https://github.com/joseph0x45/updaters)\n"
                  "Usage:\n\t"
                  "Update Zen: zen_updater -u\n");
}

int get_zen_version(char *buffer, size_t buffer_size) {
  const char *cmd = "zen --version | cut -d' ' -f3";
  FILE *fp;
  fprintf(stdout, "Getting current Zen version\n");
  fp = popen(cmd, "r");
  if (fp == NULL) {
    fprintf(stdout, "Error while getting current Zen version\n");
    perror("popen:");
    return EXIT_FAILURE;
  }
  if (fgets(buffer, buffer_size, fp) == NULL) {
    fprintf(stdout, "Error while getting current Zen version\n");
    pclose(fp);
    return EXIT_FAILURE;
  }
  pclose(fp);
  size_t len = strlen(buffer);
  if (len > 0 && buffer[len - 1] == '\n') {
    buffer[len - 1] = '\0';
  }
  return EXIT_SUCCESS;
}

typedef struct {
  char *data;
  size_t size;
} MemoryChunk;

static size_t write_memory_cb(void *contents, size_t size, size_t nmeb,
                              void *userp) {
  size_t realsize = size * nmeb;
  MemoryChunk *chunk = (MemoryChunk *)userp;
  char *ptr = realloc(chunk->data, chunk->size + realsize + 1);
  if (!ptr) {
    perror("realloc:");
    return 0;
  }
  chunk->data = ptr;
  memcpy(&(chunk->data[chunk->size]), contents, realsize);
  chunk->size += realsize;
  chunk->data[chunk->size] = 0;
  return realsize;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

int update(void) {
  CURL *curl_handle;
  int res;
  MemoryChunk chunk;
  chunk.data = malloc(1);
  fprintf(stdout, "Checking %s for new releases\n", ZEN_RELEASES);
  if (!chunk.data) {
    fprintf(stderr, "Something went wrong:");
    perror("malloc:");
    return EXIT_FAILURE;
  }
  chunk.size = 0;
  res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    fprintf(stderr, "Failed to initialize curl: %s\n", curl_easy_strerror(res));
    free(chunk.data);
    return EXIT_FAILURE;
  }
  curl_handle = curl_easy_init();
  if (!curl_handle) {
    fprintf(stderr, "Failed to initialize curl: %s\n", curl_easy_strerror(res));
    free(chunk.data);
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_URL, ZEN_RELEASES)) !=
      CURLE_OK) {
    fprintf(stderr, "Failed to set URL: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    free(chunk.data);
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION,
                              write_memory_cb)) != CURLE_OK) {
    fprintf(stderr, "Failed to set write function: %s\n",
            curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    free(chunk.data);
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA,
                              (void *)&chunk)) != CURLE_OK) {
    fprintf(stderr, "Failed to set write data: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    free(chunk.data);
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT,
                              "libcurl-agent/1.0")) != CURLE_OK) {
    fprintf(stderr, "Failed to set user agent: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    free(chunk.data);
    return EXIT_FAILURE;
  }
  res = curl_easy_perform(curl_handle);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    free(chunk.data);
    curl_global_cleanup();
    return EXIT_FAILURE;
  }
  long status_code = 0;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status_code);
  if (status_code != 200) {
    fprintf(stdout, "Expected HTTP 200 but got HTTP %ld from Github.\n",
            status_code);
    free(chunk.data);
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    return EXIT_FAILURE;
  }
  curl_easy_cleanup(curl_handle);

  cJSON *releases = cJSON_Parse(chunk.data);
  free(chunk.data);
  if (releases == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr == NULL) {
      printf("Error while parsing config: %s\n", error_ptr);
    }
    cJSON_Delete(releases);
    return EXIT_FAILURE;
  }
  if (!cJSON_IsArray(releases)) {
    fprintf(stdout, "Received bad data from Github API.\n");
    cJSON_Delete(releases);
    return EXIT_FAILURE;
  }
  cJSON *latest_release = cJSON_GetArrayItem(releases, 0);
  if (!latest_release) {
    fprintf(stdout, "Failed to get latest release from Github.\n");
    cJSON_Delete(releases);
    return EXIT_FAILURE;
  }
  cJSON *release_tag_name = cJSON_GetObjectItem(latest_release, "tag_name");
  if (cJSON_IsString(release_tag_name) &&
      release_tag_name->valuestring != NULL) {
    fprintf(stdout, "Latest version found: %s\n",
            release_tag_name->valuestring);
  }
  char current_zen_version[128];
  res = get_zen_version(current_zen_version, sizeof(current_zen_version));
  if (res == EXIT_FAILURE) {
    cJSON_Delete(releases);
    return EXIT_FAILURE;
  }
  fprintf(stdout, "Got current Zen version: %s\n", current_zen_version);

  if (strcmp(release_tag_name->valuestring, current_zen_version) == 0) {
    // versions are different! Update
    fprintf(stdout, "Zen is up to date :)\n");
    cJSON_Delete(releases);
    return EXIT_SUCCESS;
  }
  size_t len = strlen(DOWNLOAD_BASE_URL) +
               strlen(release_tag_name->valuestring) + 1 + strlen(FILE_NAME) +
               1;
  char download_url[len];
  snprintf(download_url, len, "%s%s/%s", DOWNLOAD_BASE_URL,
           release_tag_name->valuestring, FILE_NAME);
  fprintf(stdout, "Downloading latest Zen version from %s\n", download_url);

  curl_handle = curl_easy_init();
  if (!curl_handle) {
    fprintf(stderr, "Failed to initialize curl: %s\n", curl_easy_strerror(res));
    curl_global_cleanup();
    return EXIT_FAILURE;
  }
  len = strlen(DOWNLOAD_FOLDER) + strlen(FILE_NAME) + 1;
  char destination_file[len];
  snprintf(destination_file, len, "%s%s", DOWNLOAD_FOLDER, FILE_NAME);
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_URL, download_url)) !=
      CURLE_OK) {
    fprintf(stderr, "Failed to set URL: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION,
                              write_data)) != CURLE_OK) {
    fprintf(stderr, "Failed to set write function: %s\n",
            curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    return EXIT_FAILURE;
  }
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
  FILE *fp;
  fp = fopen(destination_file, "wb");
  if (!fp) {
    fprintf(stderr, "Failed to set open destination file:\n");
    perror("fopen:");
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)fp)) !=
      CURLE_OK) {
    fprintf(stderr, "Failed to set write data: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    fclose(fp);
    return EXIT_FAILURE;
  }
  if ((res = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT,
                              "libcurl-agent/1.0")) != CURLE_OK) {
    fprintf(stderr, "Failed to set user agent: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    fclose(fp);
    return EXIT_FAILURE;
  }
  res = curl_easy_perform(curl_handle);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    fclose(fp);
    return EXIT_FAILURE;
  }
  fprintf(stdout, "Successfully downloaded new Zen version at %s\n",
          destination_file);
  curl_easy_cleanup(curl_handle);
  curl_global_cleanup();
  fclose(fp);

  fprintf(stdout, "Setting file to be executable\n");
  if (chmod(destination_file, 0755) != 0) {
    fprintf(stderr, "Failed to make file executable:\n");
    perror("chmod:");
    return EXIT_FAILURE;
  }
  fprintf(stdout, "Done!\n");
  fprintf(stdout, "Move file to /usr/local/bin/zen !\n");
  res = system(
      "sudo mv /home/joseph/Downloads/zen-x86_64.AppImage /usr/local/bin/zen");
  if (res != 0) {
    fprintf(stderr, "Failed to move file to /usr/local/bin/zen:\n");
    perror("system:");
    return EXIT_FAILURE;
  }

  fprintf(stdout, "Zen is now up to date :)\n");
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }
  int opt, res = 0;
  while ((opt = getopt(argc, argv, "u")) != -1) {
    switch (opt) {
    case 'u':
      res = update();
      break;
    default:
      print_usage();
    }
  }
  return res;
}
