#include <string.h>
#include <stdbool.h>

#include "gameserver.h"
#include "cJSON.h"

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void pfds_add_client(struct pollfd *pfds[], int *size, int ncli, int sockfd, short events){
	if(ncli == *size){
		*size *= 2;
		*pfds = realloc(*pfds, sizeof(*pfds) * *size);
	}

	(*pfds)[ncli].fd = sockfd;
	(*pfds)[ncli].events = events;
	(*pfds)[ncli].revents = 0;
}

void pfds_remove_client(struct pollfd *pfds[], int *size, int ncli, int i){
	if(*size/2 > ncli && *size > 8){
		*pfds = realloc(*pfds, sizeof(*pfds) * *size/2);
		*size = *size/2;
	}
	
	if(ncli == 1) return;

	if(i >= ncli){
		return;
	}

	if(i == ncli-1){
		return;
	}
	//memory past ncli-1 is unitialized
	while(i < ncli-1){
		// the current one is the one for deletion;
		(*pfds)[i] = (*pfds)[i+1];
		i++;
	}
	return;
}


int server_socket_init(){
  int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int yes=1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("server is ready for listening\n");

  return sockfd;
}

struct client *accept_client(
	struct pollfd *queue_pfds[], 
	int *queue_size, 
	int ncli, 
	int serv_sock
)
{
	int cl_sock;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size = sizeof(their_addr);
	
	printf("waiting for connections\n");
	cl_sock = accept(serv_sock, (struct sockaddr *)&their_addr, &sin_size);
	if(cl_sock < 0){
		perror("accept");
		return NULL; //TODO check for null
	}

	pfds_add_client(queue_pfds, queue_size, ncli, cl_sock, POLLOUT);
	printf("added connection %d, number of conns: %d\n", cl_sock, ncli);

	struct client *client_info = malloc(sizeof(struct client));
	client_info->objs = NULL;
	client_info->objs_n = 0;
	client_info->tcp_sock = cl_sock;
	client_info->udp_sock = 0;
	client_info->addr_len = sizeof client_info->addr;

	client_info->ready = false;
	client_info->timed_out = false;
	client_info->udp_packet_time = 0;	

	return client_info;
}

void client_register_obj(struct client *cli, struct obj_data *obj){
	cli->objs_n++;
	cli->objs = realloc(cli->objs, sizeof(struct obj_data*) * cli->objs_n);

	if(!cli->objs){
		printf("failer to allocare memory for obj data");
		exit(-1);
	}

	cli->objs[cli->objs_n-1] = obj;
}

void obj_free(struct obj_data *obj){
	free(obj->name);
	for(int i = 0; i < obj->nfields; i++){
		free(obj->interpol_fields[i]);
	}
	free(obj->interpol_values);
	free(obj->interpol_fields);
	free(obj);
}

void client_handle_packet(struct client* cli, char *buff, size_t nbuff, int *echo_packet){
	cJSON *json = cJSON_ParseWithLength(buff, nbuff);

	if (json == NULL) {
		goto cleanup;
  }

	cJSON *echo_flag = cJSON_GetObjectItem(json, "echo");
	if(!cJSON_IsBool(echo_flag)){
		goto cleanup;
	}
	if (echo_flag->valueint){
		*echo_packet = 1;
	}

	cJSON *server_flag = cJSON_GetObjectItem(json, "server_processed");
	if(!cJSON_IsBool(server_flag)){
		printf("skipping server processing");
		goto cleanup;
	}
	if(!server_flag->valueint){
		goto cleanup;
	}

	cJSON *packet_type = cJSON_GetObjectItem(json, "type");
	if(!cJSON_IsString(packet_type) || packet_type->valuestring == NULL){
		goto cleanup;
	}
	printf("parsed packet type %s\n", packet_type->valuestring);

	cJSON *packet = cJSON_GetObjectItem(json, "packet");
	if(!cJSON_IsObject(packet)){
		goto cleanup;
	}

	if (!strcmp(packet_type->valuestring, "obj_register")){
		cJSON *obj_name = cJSON_GetObjectItem(packet, "obj_name");
		if(!cJSON_IsString(obj_name) || obj_name->valuestring == NULL){
			goto cleanup;
		}
		cJSON *fields_arr = cJSON_GetObjectItem(packet, "interpol_fields");
		if(!cJSON_IsArray(fields_arr)){
			goto cleanup;
		}
		
		struct obj_data *obj = malloc(sizeof(*obj));

		obj->interpol_values = calloc(sizeof(int), cJSON_GetArraySize(fields_arr));
		obj->interpol_fields = malloc(sizeof(char*) * cJSON_GetArraySize(fields_arr));
		for(int i = 0; i < cJSON_GetArraySize(fields_arr); i++){
			cJSON *item = cJSON_GetArrayItem(fields_arr, i);
			if(!cJSON_IsString(obj_name) || obj_name->valuestring == NULL){
				printf("bad field item\n");
				obj_free(obj);
				goto cleanup;
			}
			
			obj->interpol_fields[obj->nfields] = strdup(item->valuestring);
			obj->nfields++;
			printf("%s\n", item->valuestring);
		}

		printf("%s\n", obj_name->valuestring);

		obj->name = strdup(obj_name->valuestring);
		client_register_obj(cli, obj);

	} else if (!strcmp(packet_type->valuestring, "obj_update")){
		cJSON *obj_name = cJSON_GetObjectItem(packet, "obj_name");
		if(!cJSON_IsString(obj_name) || obj_name->valuestring == NULL){
			goto cleanup;
		}
		cJSON *fields_arr = cJSON_GetObjectItem(packet, "interpol_fields");
		if(!cJSON_IsArray(fields_arr)){
			goto cleanup;
		}

		struct obj_data *target_obj = NULL;
		for(int i = 0; i < cli->objs_n; i++){
			if(!strcmp(cli->objs[i]->name, obj_name->valuestring)){
				target_obj = cli->objs[i];
			}
		}
		if(!target_obj){
			printf("no object found\n");
			goto cleanup;
		}

		for(int i = 0; i < cJSON_GetArraySize(fields_arr); i++){
			cJSON *item = cJSON_GetArrayItem(fields_arr, i);
			if(!cJSON_IsString(obj_name) || obj_name->valuestring == NULL){
				printf("bad field item\n");
				goto cleanup;
			}

			int target_field_i = -1;
			for(int i = 0; i < target_obj->nfields; i++){
				if(!strcmp(target_obj->interpol_fields[i], item->valuestring)){
					target_field_i = i;
					break;
				}
			}

			if(target_field_i != -1){
				cJSON *val_array = cJSON_GetObjectItem(packet, "values");
				if(!cJSON_IsArray(fields_arr)){
					goto cleanup;
				}
				cJSON *target_value = cJSON_GetArrayItem(val_array, target_field_i);
				if(!cJSON_IsNumber(target_value)){
					goto cleanup;
				}
				target_obj->interpol_values[target_field_i] = target_value->valueint;
				printf("%s %d\n", target_obj->interpol_fields[target_field_i], target_value->valueint);
			}
			printf("%s\n", item->valuestring);
		}
	}

cleanup:
	const char *error_ptr = cJSON_GetErrorPtr();
	if (error_ptr != NULL) {
		//printf("Error: %s\n", error_ptr);
	}
	cJSON_Delete(json);
}

void client_init_udp_socket(int *udp_sockfd){
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(-1);
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		exit(-1);
	}
	freeaddrinfo(servinfo);

	*udp_sockfd = sockfd;

	printf("initilized udp socket\n");
}