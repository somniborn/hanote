// SPDX-FileCopyrightText: 2026 Suho Kang
// SPDX-License-Identifier: GPL-3.0-only

#include "store.h"
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <sys/stat.h>

char *store_get_path(void) {
    const char *data_dir = g_get_user_data_dir();
    char *dir = g_build_filename(data_dir, "hanote", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *path = g_build_filename(dir, "notes.json", NULL);
    g_free(dir);
    return path;
}

char *store_get_images_dir(void) {
    const char *data_dir = g_get_user_data_dir();
    char *dir = g_build_filename(data_dir, "hanote", "images", NULL);
    g_mkdir_with_parents(dir, 0755);
    return dir;
}

GList *store_load(void) {
    GList *notes = NULL;
    char *path = store_get_path();

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return NULL;
    }

    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    if (!json_parser_load_from_file(parser, path, &error)) {
        g_warning("Failed to load notes: %s", error->message);
        g_error_free(error);
        g_object_unref(parser);
        g_free(path);
        return NULL;
    }

    JsonNode *root = json_parser_get_root(parser);
    JsonArray *array = json_node_get_array(root);
    guint len = json_array_get_length(array);

    for (guint i = 0; i < len; i++) {
        JsonObject *obj = json_array_get_object_element(array, i);

        NoteData *nd = g_new0(NoteData, 1);
        nd->id     = g_strdup(json_object_get_string_member(obj, "id"));
        nd->text   = g_strdup(json_object_get_string_member(obj, "text"));
        nd->color  = g_strdup(json_object_get_string_member(obj, "color"));
        nd->font   = json_object_has_member(obj, "font")
                     ? g_strdup(json_object_get_string_member(obj, "font"))
                     : NULL;
        nd->monitor_id = json_object_has_member(obj, "monitor_id")
                         ? g_strdup(json_object_get_string_member(obj, "monitor_id"))
                         : NULL;
        nd->rel_x  = json_object_has_member(obj, "rel_x")
                     ? json_object_get_double_member(obj, "rel_x") : 0.3;
        nd->rel_y  = json_object_has_member(obj, "rel_y")
                     ? json_object_get_double_member(obj, "rel_y") : 0.3;
        nd->width  = (int)json_object_get_int_member(obj, "width");
        nd->height = (int)json_object_get_int_member(obj, "height");
        nd->images = NULL;
        nd->md_enabled = json_object_has_member(obj, "md_enabled")
                         ? json_object_get_boolean_member(obj, "md_enabled")
                         : FALSE;

        if (json_object_has_member(obj, "images")) {
            JsonArray *imgs = json_object_get_array_member(obj, "images");
            guint img_len = json_array_get_length(imgs);
            for (guint j = 0; j < img_len; j++) {
                JsonObject *io = json_array_get_object_element(imgs, j);
                NoteImage *img = note_image_new(
                    json_object_get_string_member(io, "filename"),
                    (int)json_object_get_int_member(io, "offset"),
                    (int)json_object_get_int_member(io, "width"),
                    (int)json_object_get_int_member(io, "height")
                );
                nd->images = g_list_append(nd->images, img);
            }
        }

        notes = g_list_append(notes, nd);
    }

    g_object_unref(parser);
    g_free(path);
    return notes;
}

void store_save(GList *notes) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_array(builder);

    for (GList *l = notes; l != NULL; l = l->next) {
        NoteData *nd = l->data;
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "id");
        json_builder_add_string_value(builder, nd->id);
        json_builder_set_member_name(builder, "text");
        json_builder_add_string_value(builder, nd->text);
        json_builder_set_member_name(builder, "color");
        json_builder_add_string_value(builder, nd->color);
        if (nd->font) {
            json_builder_set_member_name(builder, "font");
            json_builder_add_string_value(builder, nd->font);
        }
        if (nd->monitor_id) {
            json_builder_set_member_name(builder, "monitor_id");
            json_builder_add_string_value(builder, nd->monitor_id);
        }
        json_builder_set_member_name(builder, "rel_x");
        json_builder_add_double_value(builder, nd->rel_x);
        json_builder_set_member_name(builder, "rel_y");
        json_builder_add_double_value(builder, nd->rel_y);
        json_builder_set_member_name(builder, "width");
        json_builder_add_int_value(builder, nd->width);
        json_builder_set_member_name(builder, "height");
        json_builder_add_int_value(builder, nd->height);
        json_builder_set_member_name(builder, "md_enabled");
        json_builder_add_boolean_value(builder, nd->md_enabled);

        if (nd->images) {
            json_builder_set_member_name(builder, "images");
            json_builder_begin_array(builder);
            for (GList *il = nd->images; il; il = il->next) {
                NoteImage *img = il->data;
                json_builder_begin_object(builder);
                json_builder_set_member_name(builder, "filename");
                json_builder_add_string_value(builder, img->filename);
                json_builder_set_member_name(builder, "offset");
                json_builder_add_int_value(builder, img->offset);
                json_builder_set_member_name(builder, "width");
                json_builder_add_int_value(builder, img->width);
                json_builder_set_member_name(builder, "height");
                json_builder_add_int_value(builder, img->height);
                json_builder_end_object(builder);
            }
            json_builder_end_array(builder);
        }

        json_builder_end_object(builder);
    }

    json_builder_end_array(builder);

    JsonGenerator *gen = json_generator_new();
    json_generator_set_pretty(gen, TRUE);
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);

    char *path = store_get_path();
    GError *error = NULL;
    if (!json_generator_to_file(gen, path, &error)) {
        g_warning("Failed to save notes: %s", error->message);
        g_error_free(error);
    }

    json_node_unref(root);
    g_object_unref(gen);
    g_object_unref(builder);
    g_free(path);
}
