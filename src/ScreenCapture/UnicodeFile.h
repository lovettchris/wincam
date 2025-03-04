#pragma once

class UnicodeFile {
    HANDLE _handle = 0;
    DWORD _writeError = 0;
public:

    UnicodeFile()
    {
    }
    ~UnicodeFile() {
        CloseHandle(_handle);
    }
    int OpenFile(const std::wstring fullPath)
    {
        // Create a handle to the file
        _handle = CreateFileW(
            fullPath.c_str(),
            GENERIC_WRITE,
            0, // No sharing
            NULL, // Default security
            CREATE_ALWAYS, // Create a new file or overwrite existing one
            FILE_ATTRIBUTE_NORMAL,
            NULL // No template file
        );

        if (_handle == INVALID_HANDLE_VALUE)
        {
            return GetLastError();
        }
        return 0;
    }

    void WriteBytes(const uint8_t* buf, int buf_size)
    {
        DWORD bytesWritten = 0;
        BOOL result = WriteFile(
            _handle,
            buf,
            static_cast<DWORD>(buf_size),
            &bytesWritten,
            NULL // No overlapping
        );

        if (!result)
        {
            _writeError = GetLastError();
        }
    }

    int64_t Seek(int64_t position, int origin)
    {
        DWORD moveMethod = FILE_BEGIN;
        if (origin == SEEK_CUR) {
            moveMethod = FILE_CURRENT;
        }
        else if (origin == SEEK_END) {
            moveMethod = FILE_END;
        }

        LARGE_INTEGER liOfs = { 0 };
        liOfs.QuadPart = position;
        LARGE_INTEGER liNew = { 0 };
        SetFilePointerEx(_handle, liOfs, &liNew, moveMethod);
        return liNew.QuadPart;
    }

    int64_t Position()
    {
        LARGE_INTEGER liOfs = { 0 };
        LARGE_INTEGER liNew = { 0 };
        SetFilePointerEx(_handle, liOfs, &liNew, FILE_CURRENT);
        return liNew.QuadPart;
    }
};
