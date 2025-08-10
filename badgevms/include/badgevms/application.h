/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "badgevms/pathfuncs.h"

#include <stdbool.h>
#include <stdio.h>

#include <sys/types.h>

typedef enum {
    APPLICATION_SOURCE_UNKNOWN,
    APPLICATION_SOURCE_BADGEHUB,
    APPLICATION_SOURCE_MAX,
} application_source_t;

typedef struct {
    char const                *unique_identifier; // Unique identifier
    char const                *name;              // Human friendly
    char const                *author;            // Author (if known)
    char const                *version;           // Installed version
    char const                *interpreter;       // Interpreter if needed
    char const                *metadata_file;     // Metadata file (if any) relative to install path
    char const                *installed_path;    // Physical install location
    char const                *binary_path;       // Physical main binary location
    application_source_t const source;            // Where did this application come from
} application_t;

typedef struct application_list *application_list_handle;

// Launch an application by name, returns the pid of the application or -1 on failure.
pid_t application_launch(char const *unique_identifier);

// Create a new application. You will get an application_t* back if it succeeded or NULL if it failed.
// This creates the application directory and metadata file
application_t *application_create(
    char const          *unique_identifier,
    char const          *name,
    char const          *author,
    char const          *version,
    char const          *interpreter,
    application_source_t source
);

// Set the metadata file for the application. Note that BadgeVMS does not care what is in this file or whether or not it
// exists. The path is relative to APP: path. See application_create_file() for an explanation.
bool application_set_metadata(application_t *application, char const *metadata_file);

// Set the path to the main executable of the application. The executable path is relative to the APP: path. See
// application_create_file() for an explanation.
bool application_set_binary_path(application_t *application, char const *binary_path);

// Change the version of an application_t instance
bool application_set_version(application_t *application, char const *version);

// Change the author of an application_t instance
bool application_set_author(application_t *application, char const *author);

// Change the name of an application_t instance
bool application_set_name(application_t *application, char const *name);

// Change the interpreter of an application_t instance
bool application_set_interpreter(application_t *application, char const *interpreter);

// Remove an application and all its associated files. Might fail.
bool application_destroy(application_t *application);

// Create a file in an application installation. file_path should be a relative path like
// filename.ext to be placed in APP:filename.ext or
// [dir.subdir]filename.ext to be placed in APP:[dir.subdir]filename.ext
// subdirectories are automatically created if needed
FILE *application_create_file(application_t *application, char const *file_path);

// Generate an absolute path to install the file with the given name into the application
// see application_create_file for details. Subdirectories will be created even if the file
// is never created. Note that it is not necessary to additionally also call
// application_create_file() if you create the file based on the returned name.
// The caller should free() the string.
char *application_create_file_string(application_t *application, char const *file_path);

// Query the list of installed applications, giving a list and the first application in the list.
// out can be NULL.
application_list_handle application_list(application_t **out);

// Get the next application_t* in the list, application*'s do not need to be separately
// application_free()'d they will be freed by application_list_close()
application_t *application_list_get_next(application_list_handle list);

// Close the application list and free associated resources
void application_list_close(application_list_handle list);

// Get an application by unique ID, or NULL if the application isn't found
application_t *application_get(char const *unique_identifier);

// Free an application_t, must be called for each application_t* returned by the api
void application_free(application_t *application);

// Is the requested application running?
bool application_is_running(char const *unique_identifier);
