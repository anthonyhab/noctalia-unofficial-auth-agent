#include "ipc-client.h"

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <json-glib/json-glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static gchar *
get_socket_path (void)
{
    const gchar *runtime_dir = g_get_user_runtime_dir ();
    return g_build_filename (runtime_dir, "noctalia-polkit-agent.sock", NULL);
}

static GSocketConnection *
connect_to_socket (void)
{
    g_autoptr(GSocketClient) client = NULL;
    g_autoptr(GSocketAddress) address = NULL;
    g_autofree gchar *socket_path = NULL;
    GSocketConnection *connection = NULL;
    GError *error = NULL;

    socket_path = get_socket_path ();

    client = g_socket_client_new ();
    address = g_unix_socket_address_new (socket_path);

    connection = g_socket_client_connect (client,
                                          G_SOCKET_CONNECTABLE (address),
                                          NULL, &error);

    if (error) {
        g_debug ("Failed to connect to socket %s: %s", socket_path, error->message);
        g_error_free (error);
        return NULL;
    }

    return connection;
}

static gchar *
send_simple_command (const gchar *command)
{
    g_autoptr(GSocketConnection) connection = NULL;
    GOutputStream *output;
    GInputStream *input;
    GError *error = NULL;
    g_autofree gchar *request = NULL;
    gchar buffer[4096];
    gssize bytes_read;

    connection = connect_to_socket ();
    if (!connection)
        return NULL;

    output = g_io_stream_get_output_stream (G_IO_STREAM (connection));
    input = g_io_stream_get_input_stream (G_IO_STREAM (connection));

    request = g_strdup_printf ("%s\n", command);

    if (!g_output_stream_write_all (output, request, strlen (request),
                                    NULL, NULL, &error)) {
        g_debug ("Failed to write command: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    bytes_read = g_input_stream_read (input, buffer, sizeof(buffer) - 1,
                                      NULL, &error);

    if (bytes_read < 0) {
        g_debug ("Failed to read response: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    /* Trim trailing newline */
    gchar *newline = strchr (buffer, '\n');
    if (newline)
        *newline = '\0';

    return g_strdup (buffer);
}

gboolean
noctalia_ipc_ping (void)
{
    g_autofree gchar *response = send_simple_command ("PING");
    return response && g_strcmp0 (response, "PONG") == 0;
}

gboolean
noctalia_ipc_send_keyring_request (const gchar *cookie,
                                   const gchar *title,
                                   const gchar *message,
                                   const gchar *description,
                                   gboolean     password_new,
                                   gchar      **out_password)
{
    g_autoptr(GSocketConnection) connection = NULL;
    GOutputStream *output;
    GInputStream *input;
    GError *error = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonGenerator) generator = NULL;
    g_autoptr(JsonNode) root = NULL;
    g_autofree gchar *json_str = NULL;
    g_autofree gchar *request = NULL;
    gchar buffer[8192];
    gssize bytes_read;

    *out_password = NULL;

    /* Build JSON payload */
    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "cookie");
    json_builder_add_string_value (builder, cookie);

    json_builder_set_member_name (builder, "title");
    json_builder_add_string_value (builder, title ? title : "Unlock Keyring");

    json_builder_set_member_name (builder, "message");
    json_builder_add_string_value (builder, message ? message : "Password required");

    if (description) {
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, description);
    }

    json_builder_set_member_name (builder, "password_new");
    json_builder_add_boolean_value (builder, password_new);

    json_builder_end_object (builder);

    root = json_builder_get_root (builder);
    generator = json_generator_new ();
    json_generator_set_root (generator, root);
    json_str = json_generator_to_data (generator, NULL);

    /* Send KEYRING_REQUEST command with JSON payload */
    request = g_strdup_printf ("KEYRING_REQUEST\n%s\n", json_str);

    connection = connect_to_socket ();
    if (!connection) {
        g_warning ("Failed to connect to noctalia-polkit socket");
        return FALSE;
    }

    output = g_io_stream_get_output_stream (G_IO_STREAM (connection));
    input = g_io_stream_get_input_stream (G_IO_STREAM (connection));

    if (!g_output_stream_write_all (output, request, strlen (request),
                                    NULL, NULL, &error)) {
        g_warning ("Failed to send keyring request: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    /* Block waiting for response (password or cancel)
     * Response format: OK\n<password>\n or CANCEL\n or ERROR\n */
    bytes_read = g_input_stream_read (input, buffer, sizeof(buffer) - 1,
                                      NULL, &error);

    if (bytes_read < 0) {
        g_warning ("Failed to read response: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    buffer[bytes_read] = '\0';

    /* Parse response */
    if (g_str_has_prefix (buffer, "OK\n")) {
        /* Password is on the second line */
        gchar *password_start = buffer + 3;
        gchar *newline = strchr (password_start, '\n');
        if (newline)
            *newline = '\0';
        *out_password = g_strdup (password_start);
        return TRUE;
    } else if (g_str_has_prefix (buffer, "CANCEL")) {
        g_debug ("Keyring request cancelled by user");
        return FALSE;
    }

    g_warning ("Unexpected response from noctalia-polkit: %s", buffer);
    return FALSE;
}

gboolean
noctalia_ipc_send_confirm_request (const gchar *cookie,
                                   const gchar *title,
                                   const gchar *message,
                                   const gchar *description)
{
    g_autoptr(GSocketConnection) connection = NULL;
    GOutputStream *output;
    GInputStream *input;
    GError *error = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonGenerator) generator = NULL;
    g_autoptr(JsonNode) root = NULL;
    g_autofree gchar *json_str = NULL;
    g_autofree gchar *request = NULL;
    gchar buffer[4096];
    gssize bytes_read;

    /* Build JSON payload */
    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "cookie");
    json_builder_add_string_value (builder, cookie);

    json_builder_set_member_name (builder, "title");
    json_builder_add_string_value (builder, title ? title : "Confirm");

    json_builder_set_member_name (builder, "message");
    json_builder_add_string_value (builder, message ? message : "Please confirm");

    if (description) {
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, description);
    }

    json_builder_set_member_name (builder, "confirm_only");
    json_builder_add_boolean_value (builder, TRUE);

    json_builder_end_object (builder);

    root = json_builder_get_root (builder);
    generator = json_generator_new ();
    json_generator_set_root (generator, root);
    json_str = json_generator_to_data (generator, NULL);

    request = g_strdup_printf ("KEYRING_CONFIRM\n%s\n", json_str);

    connection = connect_to_socket ();
    if (!connection)
        return FALSE;

    output = g_io_stream_get_output_stream (G_IO_STREAM (connection));
    input = g_io_stream_get_input_stream (G_IO_STREAM (connection));

    if (!g_output_stream_write_all (output, request, strlen (request),
                                    NULL, NULL, &error)) {
        g_warning ("Failed to send confirm request: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    bytes_read = g_input_stream_read (input, buffer, sizeof(buffer) - 1,
                                      NULL, &error);

    if (bytes_read < 0) {
        g_warning ("Failed to read confirm response: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    buffer[bytes_read] = '\0';

    return g_str_has_prefix (buffer, "CONFIRMED");
}

void
noctalia_ipc_send_cancel (const gchar *cookie)
{
    g_autofree gchar *command = g_strdup_printf ("CANCEL %s", cookie);
    g_autofree gchar *response = send_simple_command (command);
    /* Ignore response - best effort cancel */
}
