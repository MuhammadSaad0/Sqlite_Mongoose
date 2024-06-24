#include "mongoose.h" 
#include <sqlite3.h>

#define BUFFER_SIZE 1024

typedef struct {
    char *json_str;
    size_t len;
} json_buffer_t;

static int print_table(void *data, int argc, char **argv, char **azColName) {
    json_buffer_t *buffer = (json_buffer_t *)data;
    size_t needed = buffer->len + BUFFER_SIZE;
    buffer->json_str = realloc(buffer->json_str, needed);
    
    if (buffer->len == 0) {
        strcat(buffer->json_str, "[");
    } else {
        strcat(buffer->json_str, ",");
    }

    strcat(buffer->json_str, "{");

    for (int i = 0; i < argc; i++) {
        char item[BUFFER_SIZE];
        snprintf(item, sizeof(item), "\"%s\":\"%s\"", azColName[i], argv[i] ? argv[i] : "NULL");
        strcat(buffer->json_str, item);
        if (i < argc - 1) {
            strcat(buffer->json_str, ",");
        }
    }

    strcat(buffer->json_str, "}");
    buffer->len = strlen(buffer->json_str);

    return 0;
}

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {  // New HTTP request received
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;  // Parsed HTTP request
    if (mg_match(hm->uri, mg_str("/api/run_query"), NULL)) {   
      struct mg_str json = hm->body; 
      char* query = NULL;
      char *zErrMsg = 0;
      int rc;
      query = mg_json_get_str(json, "$.query");
      if(query != NULL){
        fprintf(stderr, "Extracted query: %s\n", query);
        rc = sqlite3_exec(c->fn_data, query, NULL, 0, &zErrMsg);
        if(rc != SQLITE_OK) {
          fprintf(stderr, "Error running query\n %s\n", zErrMsg);
        }else{
          mg_http_reply(c, 200, "", "Query run successfully");    
        }
        mg_http_reply(c, 400, "", "{%m:%d}\n", MG_ESC("status"), 400);    
      }        
      mg_http_reply(c, 200, "", "{%m:%d}\n", MG_ESC("status"), 200);   

    } if (mg_match(hm->uri, mg_str("/api/select_table"), NULL)) {
      struct mg_str json = hm->body;
      char *table_name = mg_json_get_str(json, "$.table");
      int rc;
      json_buffer_t buffer = { .json_str = malloc(BUFFER_SIZE), .len = 0 };
      if (table_name != NULL) {
          char query[512];
          snprintf(query, sizeof(query), "SELECT * FROM %s", table_name);
          rc = sqlite3_exec(c->fn_data, query, print_table, (void *)&buffer, NULL);
          if (rc != SQLITE_OK) {
              fprintf(stderr, "Error running select query\n");
              mg_http_reply(c, 500, "", "{%m:%d, %m:%m}\n", MG_ESC("status"), 500, MG_ESC("error"), MG_ESC("Error running select query"));
          } else {
              strcat(buffer.json_str, "]");
              mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", buffer.json_str);
          }
          } else {
              mg_http_reply(c, 400, "", "{%m:%d, %m:%m}\n", MG_ESC("status"), 400, MG_ESC("error"), MG_ESC("Invalid table name"));
          }
          free(buffer.json_str);
          free(table_name);
        } else {
      struct mg_http_serve_opts opts = {.root_dir = "."};  
      mg_http_serve_dir(c, hm, &opts);                     
    }
  }
}
int main(void) {
  struct mg_mgr mgr;  // Declare event manager
  mg_mgr_init(&mgr);  // Initialise event manager
  sqlite3 *db;
  int rc;
  rc = sqlite3_open("test.db", &db);
  if(rc) {
    fprintf(stderr, "Cannot open database\n");
    exit(1);
  }else{
    printf("Database opened successfully\n");
  }
  mg_http_listen(&mgr, "http://0.0.0.0:8000" , fn, db);  // Setup listener
  for (;;) {          // Run an infinite event loop
    mg_mgr_poll(&mgr, 1000);
  }
  sqlite3_close(db);
  return 0;
}