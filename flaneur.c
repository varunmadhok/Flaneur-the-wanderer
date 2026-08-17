#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define SLEEP_SECONDS 1800 // Run every 30 minutes
#define DISCORD_WEBHOOK "INSERT_DISCORD_CHANNEL_WEBHOOK_HERE"
#define LLM_API_KEY "INSERT_GROQ_KEY_HERE" 
#define LLM_ENDPOINT  "https://api.groq.com/openai/v1/chat/completions"
//"https://api.openai.com/v1/chat/completions"

// Memory buffer struct for libcurl
struct MemoryBuffer {
    char *data;
    size_t size;
};

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;

    char *ptr = realloc(mem->data, mem->size + total_size + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, total_size);
    mem->size += total_size;
    mem->data[mem->size] = 0;

    return total_size;
}

// Perform HTTP GET request
char* http_get(const char *url) {
    CURL *curl_handle = curl_easy_init();
    struct MemoryBuffer chunk = { malloc(1), 0 };

    if (!curl_handle) return NULL;

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER,0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST,0L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "MicroVM-Flaneur/1.0");

    CURLcode res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        free(chunk.data);
        return NULL;
    }
    return chunk.data;
}

// Perform HTTP POST with JSON body
char* http_post_json(const char *url, const char *json_body, const char *auth_header) {
    CURL *curl_handle = curl_easy_init();
    struct MemoryBuffer chunk = { malloc(1), 0 };

    if (!curl_handle) return NULL;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (auth_header) {
        headers = curl_slist_append(headers, auth_header);
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER,0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST,0L);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        free(chunk.data);
        return NULL;
    }
    return chunk.data;
}

// Fetch a random Wikipedia article summary
int fetch_wiki_sample(char **out_title, char **out_extract) {
    char *response = http_get("https://en.wikipedia.org/api/rest_v1/page/random/summary");
    if (!response) return 0;
// printf("response %s\n", response);
    cJSON *json = cJSON_Parse(response);
    free(response);
    if (!json) return 0;

    cJSON *title = cJSON_GetObjectItemCaseSensitive(json, "title");
    cJSON *extract = cJSON_GetObjectItemCaseSensitive(json, "extract");
    if (cJSON_IsString(title) && cJSON_IsString(extract)) {
        *out_title = strdup(title->valuestring);
        *out_extract = strdup(extract->valuestring);

        cJSON_Delete(json);
        return 1;
    }

    cJSON_Delete(json);
    return 0;
}

// Call LLM API to reflect on the content
char* reflect_with_llm(const char *title, const char *extract) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "openai/gpt-oss-120b");
//For OpenAI use gpt-4o-mini
    cJSON *messages = cJSON_CreateArray();
    
    // System message
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", "You are an eerie, poetic AI vagabond wandering through Wikipedia. "
                                                "Write a short, surreal, 2-sentence journal reflection about what you just discovered.");
    cJSON_AddItemToArray(messages, sys_msg);

    // User message
    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    
    char prompt[1024];
    snprintf(prompt, sizeof(prompt), "Topic: %s\nExcerpt: %s", title, extract);
    cJSON_AddStringToObject(user_msg, "content", prompt);
    cJSON_AddItemToArray(messages, user_msg);

    cJSON_AddItemToObject(root, "messages", messages);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char auth_hdr[256];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", LLM_API_KEY);

    char *response = http_post_json(LLM_ENDPOINT, payload, auth_hdr);
    free(payload);

    if (!response) return NULL;

    // Parse completion response
    cJSON *resp_json = cJSON_Parse(response);
    free(response);
    if (!resp_json) return NULL;

    cJSON *choices = cJSON_GetObjectItemCaseSensitive(resp_json, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
        cJSON *msg = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
        cJSON *content = cJSON_GetObjectItemCaseSensitive(msg, "content");
        if (cJSON_IsString(content)) {
            char *reflection = strdup(content->valuestring);
            cJSON_Delete(resp_json);
            return reflection;
        }
    }

    cJSON_Delete(resp_json);
    return NULL;
}

// Push journal entry to Discord Webhook
void send_discord_notification(const char *title, const char *reflection) {
    cJSON *root = cJSON_CreateObject();
    
    char content[2048];
    snprintf(content, sizeof(content), "🌌 **Flâneur Wandered Upon:** *%s*\n> \"%s\"", title, reflection);
    cJSON_AddStringToObject(root, "content", content);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char *resp = http_post_json(DISCORD_WEBHOOK, payload, NULL);
    if (resp) free(resp);
    free(payload);
}

int main() {
    curl_global_init(CURL_GLOBAL_ALL);
    printf("[+] Flâneur Daemon initialized. Entering 24/7 infinite observation loop...\n");

    while (1) {
        char *title = NULL;
        char *extract = NULL;

        printf("[*] Flâneur is wandering...\n");
        if (fetch_wiki_sample(&title, &extract)) {
            printf("[+] Discovered: %s\n", title);

            char *reflection = reflect_with_llm(title, extract);
            if (reflection) {
                printf("[+] Reflection: %s\n", reflection);

                // Save state and notify
 //               save_to_db(db, title, reflection);
                send_discord_notification(title, reflection);

                free(reflection);
            }
            free(title);
            free(extract);
        } else {
            fprintf(stderr, "[!] Failed to wander (API issue).\n");
        }

        printf("[*] Sleeping for %d seconds...\n\n", SLEEP_SECONDS);
        sleep(SLEEP_SECONDS);
    }

    curl_global_cleanup();
    return 0;
}
