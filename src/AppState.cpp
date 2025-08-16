#include "AppState.h"

// Define custom events
const Event BOOK_LOAD_SUCCESS = Event::Special("BOOK_LOAD_SUCCESS");
const Event BOOK_LOAD_FAILURE = Event::Special("BOOK_LOAD_FAILURE");