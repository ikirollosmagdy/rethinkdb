#include "server.hpp"
#include <sys/stat.h>

server_t::server_t(cmd_config_t *cmd_config, thread_pool_t *thread_pool)
    : cmd_config(cmd_config), thread_pool(thread_pool),
      log_controller(cmd_config), store(cmd_config), conn_acceptor(this) { }

void server_t::do_start() {
    
    printf("Physical cores: %d\n", get_cpu_count());
    printf("Number of threads: %d\n", cmd_config->n_workers);
    printf("Total RAM: %ldMB\n", get_total_ram() / 1024 / 1024);
    printf("Free RAM: %ldMB (%.2f%%)\n",
           get_available_ram() / 1024 / 1024,
           (double)get_available_ram() / (double)get_total_ram() * 100.0f);
    
    do_start_loggers();
}

void server_t::do_start_loggers() {
    printf("Starting loggers...\n");
    if (log_controller.start(this)) on_logger_ready();
}

void server_t::on_logger_ready() {
    do_start_store();
}

void server_t::do_start_store() {

    printf("Shard factor: %d\n", cmd_config->n_slices);
    printf("Max cache memory usage: %lldMB\n", (long long int)(cmd_config->max_cache_size / 1024 / 1024));
    
    if (strncmp(cmd_config->db_file_name, DATA_DIRECTORY, strlen(DATA_DIRECTORY)) == 0) {
        mkdir(DATA_DIRECTORY, 0777);
    }
    
    printf("Starting cache...\n");
    if (store.start(this)) on_store_ready();
}

void server_t::on_store_ready() {
    do_start_conn_acceptor();
}

void server_t::do_start_conn_acceptor() {
    
    conn_acceptor.start();
    
    printf("Server started.\n");
    
    interrupt_message.server = this;
    thread_pool->set_interrupt_message(&interrupt_message);
}

void server_t::shutdown() {
    thread_pool->set_interrupt_message(NULL);
    if (continue_on_cpu(home_cpu, &interrupt_message))
        call_later_on_this_cpu(&interrupt_message);
}

void server_t::do_shutdown() {
    printf("Shutting down.\n");
    do_shutdown_conn_acceptor();
}

void server_t::do_shutdown_conn_acceptor() {
    printf("Shutting down connections...\n");
    if (conn_acceptor.shutdown(this)) on_conn_acceptor_shutdown();
}

void server_t::on_conn_acceptor_shutdown() {
    do_shutdown_store();
}

void server_t::do_shutdown_store() {
    printf("Shutting down cache...\n");
    if (store.shutdown(this)) on_store_shutdown();
}

void server_t::on_store_shutdown() {
    do_shutdown_loggers();
}

void server_t::do_shutdown_loggers() {
    printf("Shutting down loggers...\n");
    if (log_controller.shutdown(this)) do_stop_threads();
}

void server_t::on_logger_shutdown() {
    do_stop_threads();
}

void server_t::do_stop_threads() {
    
    // This returns immediately, but will cause all of the threads to stop after we
    // return to the event queue.
    thread_pool->shutdown();
    delete this;
}
