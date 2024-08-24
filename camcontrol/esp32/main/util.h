#pragma once

struct cJSON;

class cJSON_wrapper
{
public:
    cJSON_wrapper(cJSON*& json);

    ~cJSON_wrapper();

private:
    cJSON*& json;
};

class cJSON_Print_wrapper
{
public:
    cJSON_Print_wrapper(char*& json);

    ~cJSON_Print_wrapper();

private:
    char*& json;
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
