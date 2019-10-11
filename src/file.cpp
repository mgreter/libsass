// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#if defined (_MSC_VER) // Visual studio
#define thread_local __declspec( thread )
#elif defined (__GCC__) // GCC
#define thread_local __thread
#endif

#ifdef _WIN32
# ifdef __MINGW32__
#  ifndef off64_t
#   define off64_t _off64_t    /* Workaround for http://sourceforge.net/p/mingw/bugs/2024/ */
#  endif
# endif
# include <direct.h>
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#else
# include <unistd.h>
#endif
#include <cstdio>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <sys/stat.h>
#include "file.hpp"
#include "context.hpp"
#include "utf8_string.hpp"
#include "sass_functions.hpp"
#include "error_handling.hpp"
#include "util.hpp"
#include "util_string.hpp"

#ifdef _WIN32
# include <windows.h>

# ifdef _MSC_VER
# include <codecvt>
inline static std::string wstring_to_string(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wchar_converter;
    return wchar_converter.to_bytes(wstr);
}
# else // mingw(/gcc) does not support C++11's codecvt yet.
inline static Sass::sass::string wstring_to_string(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    Sass::sass::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
# endif
#endif

namespace Sass {
  namespace File {

    // return the current directory
    // always with forward slashes
    // always with trailing slash
    sass::string get_cwd()
    {
      const size_t wd_len = 4096;
      #ifndef _WIN32
        char wd[wd_len];
        char* pwd = getcwd(wd, wd_len);
        // we should check error for more detailed info (e.g. ENOENT)
        // http://man7.org/linux/man-pages/man2/getcwd.2.html#ERRORS
        if (pwd == NULL) throw Exception::OperationError("cwd gone missing");
        sass::string cwd = pwd;
      #else
        wchar_t wd[wd_len];
        wchar_t* pwd = _wgetcwd(wd, wd_len);
        if (pwd == NULL) throw Exception::OperationError("cwd gone missing");
        sass::string cwd = wstring_to_string(pwd).c_str();
        //convert backslashes to forward slashes
        replace(cwd.begin(), cwd.end(), '\\', '/');
      #endif
      if (cwd[cwd.length() - 1] != '/') cwd += '/';
      return cwd;
    }


    /* This might not compile on older clang */
    static std::unordered_map<
      sass::string, bool> cached;

    // test if path exists and is a file
    // ToDo: Move the cache to context for thread safety
    bool file_exists(const sass::string& path, const sass::string& CWD)
    {
      auto it = cached.find(path);
      if (it != cached.end()) {
        // std::cerr << "cached reg " << path << "\n";
        return it->second;
      }
      #ifdef _WIN32
        wchar_t resolved[32768];
        // windows unicode filepaths are encoded in utf16
        sass::string abspath(join_paths(CWD, path));
        if (!(abspath[0] == '/' && abspath[1] == '/')) {
          abspath = "//?/" + abspath;
        }
        // it = cached.find(abspath);
        // if (it != cached.end()) {
        //   std::cerr << "cached abs " << abspath << "\n";
        //   return it->second;
        // }
        std::wstring wpath(UTF_8::convert_to_utf16(abspath));
        std::replace(wpath.begin(), wpath.end(), '/', '\\');
        DWORD rv = GetFullPathNameW(wpath.c_str(), 32767, resolved, NULL);
        if (rv > 32767) throw Exception::OperationError("Path is too long");
        if (rv == 0) throw Exception::OperationError("Path could not be resolved");
        // This call is pretty expensive (3%) -> cache in umap?
        DWORD dwAttrib = GetFileAttributesW(resolved);
        bool result = (dwAttrib != INVALID_FILE_ATTRIBUTES
          && (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)));
        // cached[abspath] = result;
        cached[path] = result;
        return result;
      #else
        struct stat st_buf;
        bool result = (stat (path.c_str(), &st_buf) == 0)
          && (!S_ISDIR (st_buf.st_mode));
        cached[path] = result;
        return result;
#endif
    }

    // return if given path is absolute
    // works with *nix and windows paths
    bool is_absolute_path(const sass::string& path)
    {
      #ifdef _WIN32
        if (path.length() >= 3 && Util::ascii_isalpha(path[0]) && path[1] == ':') return true;
      #endif
      size_t i = 0;
      // check if we have a protocol
      if (path[i] && Util::ascii_isalpha(static_cast<unsigned char>(path[i]))) {
        // skip over all alphanumeric characters
        while (path[i] && Util::ascii_isalnum(static_cast<unsigned char>(path[i]))) ++i;
        i = i && path[i] == ':' ? i + 1 : 0;
      }
      return path[i] == '/';
    }

    // helper function to find the last directory seperator
    inline size_t find_last_folder_separator(const sass::string& path, size_t limit = sass::string::npos)
    {
      size_t pos;
      size_t pos_p = path.find_last_of('/', limit);
      #ifdef _WIN32
        size_t pos_w = path.find_last_of('\\', limit);
      #else
        size_t pos_w = sass::string::npos;
      #endif
      if (pos_p != sass::string::npos && pos_w != sass::string::npos) {
        pos = std::max(pos_p, pos_w);
      }
      else if (pos_p != sass::string::npos) {
        pos = pos_p;
      }
      else {
        pos = pos_w;
      }
      return pos;
    }

    // return only the directory part of path
    sass::string dir_name(const sass::string& path)
    {
      size_t pos = find_last_folder_separator(path);
      if (pos == sass::string::npos) return "";
      else return path.substr(0, pos+1);
    }

    // return only the filename part of path
    sass::string base_name(const sass::string& path)
    {
      size_t pos = find_last_folder_separator(path);
      if (pos == sass::string::npos) return path;
      else return path.substr(pos+1);
    }

    // do a logical clean up of the path
    // no physical check on the filesystem
    sass::string make_canonical_path (sass::string path)
    {

      // declarations
      size_t pos;

      #ifdef _WIN32
        //convert backslashes to forward slashes
        replace(path.begin(), path.end(), '\\', '/');
      #endif

      pos = 0; // remove all self references inside the path string
      while((pos = path.find("/./", pos)) != sass::string::npos) path.erase(pos, 2);

      // remove all leading and trailing self references
      while(path.size() >= 2 && path[0] == '.' && path[1] == '/') path.erase(0, 2);
      while((pos = path.length()) > 1 && path[pos - 2] == '/' && path[pos - 1] == '.') path.erase(pos - 2);


      size_t proto = 0;
      // check if we have a protocol
      if (path[proto] && Util::ascii_isalpha(static_cast<unsigned char>(path[proto]))) {
        // skip over all alphanumeric characters
        while (path[proto] && Util::ascii_isalnum(static_cast<unsigned char>(path[proto++]))) {}
        // then skip over the mandatory colon
        if (proto && path[proto] == ':') ++ proto;
      }

      // then skip over start slashes
      while (path[proto++] == '/') {}

      pos = proto; // collapse multiple delimiters into a single one
      while((pos = path.find("//", pos)) != sass::string::npos) path.erase(pos, 1);

      return path;

    }

    // join two path segments cleanly together
    // but only if right side is not absolute yet
    // Can we avoid the two string copies?
    // OK, one copy is needed anyway
    sass::string join_paths(sass::string l, sass::string r)
    {

      #if defined _WIN32 || defined EMSCRIPTEN
        // convert Windows backslashes to URL forward slashes
        replace(l.begin(), l.end(), '\\', '/');
        replace(r.begin(), r.end(), '\\', '/');
      #endif

      if (l.empty()) return r;
      if (r.empty()) return l;

      if (is_absolute_path(r)) return r;
      if (l[l.length()-1] != '/') l += '/';

      // this does a logical cleanup of the right hand path
      // Note that this does collapse x/../y sections into y.
      // This is by design. If /foo on your system is a symlink
      // to /bar/baz, then /foo/../cd is actually /bar/cd,
      // not /cd as a naive ../ removal would give you.
      // will only work on leading double dot dirs on rhs
      // therefore it is safe if lhs is already resolved cwd
      while ((r.length() > 3) && ((r.substr(0, 3) == "../") || (r.substr(0, 3)) == "..\\")) {
        size_t L = l.length(), pos = find_last_folder_separator(l, L - 2);
        bool is_slash = pos + 2 == L && (l[pos+1] == '/' || l[pos+1] == '\\');
        bool is_self = pos + 3 == L && (l[pos+1] == '.');
        if (!is_self && !is_slash) r = r.substr(3);
        else if (pos == sass::string::npos) break;
        l = l.substr(0, pos == sass::string::npos ? pos : pos + 1);
      }

      return l + r;
    }

    sass::string path_for_console(const sass::string& rel_path, const sass::string& abs_path, const sass::string& orig_path)
    {
      // magic algorith goes here!!

    // std::cerr << "rel " << rel_path << "\n";
    // std::cerr << "abs " << abs_path << "\n";
    // std::cerr << "pstate " << orig_path << "\n";

    // if the file is outside this directory show the absolute path
      if (rel_path.substr(0, 3) == "../") {
        return orig_path;
      }
      // this seems to work most of the time
      return abs_path == orig_path ? rel_path : rel_path;
    }

    // create an absolute path by resolving relative paths with cwd
    sass::string rel2abs(const sass::string& path, const sass::string& base, const sass::string& CWD)
    {
      return make_canonical_path(join_paths(join_paths(CWD + "/", base + "/"), path));
    }

    // create a path that is relative to the given base directory
    // path and base will first be resolved against cwd to make them absolute
    sass::string abs2rel(const sass::string& path, const sass::string& base, const sass::string& CWD)
    {

      sass::string abs_path = rel2abs(path, CWD, CWD);
      sass::string abs_base = rel2abs(base, CWD, CWD);

      size_t proto = 0;
      // check if we have a protocol
      if (path[proto] && Util::ascii_isalpha(static_cast<unsigned char>(path[proto]))) {
        // skip over all alphanumeric characters
        while (path[proto] && Util::ascii_isalnum(static_cast<unsigned char>(path[proto++]))) {}
        // then skip over the mandatory colon
        if (proto && path[proto] == ':') ++ proto;
      }

      // distinguish between windows absolute paths and valid protocols
      // we assume that protocols must at least have two chars to be valid
      if (proto && path[proto++] == '/' && proto > 3) return path;

      #ifdef _WIN32
        // absolute link must have a drive letter, and we know that we
        // can only create relative links if both are on the same drive
        if (abs_base[0] != abs_path[0]) return abs_path;
      #endif

      sass::string stripped_uri = "";
      sass::string stripped_base = "";

      size_t index = 0;
      size_t minSize = std::min(abs_path.size(), abs_base.size());
      for (size_t i = 0; i < minSize; ++i) {
        #ifdef FS_CASE_SENSITIVE
          if (abs_path[i] != abs_base[i]) break;
        #else
          // compare the charactes in a case insensitive manner
          // windows fs is only case insensitive in ascii ranges
          if (Util::ascii_tolower(static_cast<unsigned char>(abs_path[i])) !=
              Util::ascii_tolower(static_cast<unsigned char>(abs_base[i]))) break;
        #endif
        if (abs_path[i] == '/') index = i + 1;
      }
      for (size_t i = index; i < abs_path.size(); ++i) {
        stripped_uri += abs_path[i];
      }
      for (size_t i = index; i < abs_base.size(); ++i) {
        stripped_base += abs_base[i];
      }

      size_t left = 0;
      size_t directories = 0;
      for (size_t right = 0; right < stripped_base.size(); ++right) {
        if (stripped_base[right] == '/') {
          if (stripped_base.substr(left, 2) != "..") {
            ++directories;
          }
          else if (directories > 1) {
            --directories;
          }
          else {
            directories = 0;
          }
          left = right + 1;
        }
      }

      sass::string result = "";
      for (size_t i = 0; i < directories; ++i) {
        result += "../";
      }
      result += stripped_uri;

      return result;
    }

    // Resolution order for ambiguous imports:
    // (1) filename as given
    // (2) underscore + given
    // (3) underscore + given + extension
    // (4) given + extension
    // (5) given + _index.scss
    // (6) given + _index.sass
    sass::vector<Include> resolve_includes(const sass::string& root, const sass::string& file, const sass::string& CWD, const std::vector<sass::string>& exts)
    {
      sass::string filename = join_paths(root, file);
      // split the filename
      sass::string base(dir_name(file));
      sass::string name(base_name(file));
      sass::vector<Include> includes;
      // create full path (maybe relative)
      sass::string rel_path(join_paths(base, name));
      sass::string abs_path(join_paths(root, rel_path));
      if (file_exists(abs_path, CWD)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      // next test variation with underscore
      rel_path = join_paths(base, "_" + name);
      abs_path = join_paths(root, rel_path);
      if (file_exists(abs_path, CWD)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      // next test exts plus underscore
      for(auto ext : exts) {
        rel_path = join_paths(base, "_" + name + ext);
        abs_path = join_paths(root, rel_path);
        if (file_exists(abs_path, CWD)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      }
      // next test plain name with exts
      for(auto ext : exts) {
        rel_path = join_paths(base, name + ext);
        abs_path = join_paths(root, rel_path);
        if (file_exists(abs_path, CWD)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      }
      // index files
      if (includes.size() == 0) {
        // ignore directories that look like @import'able filename
        for(auto ext : exts) {
          if (ends_with(name, ext)) return includes;
        }
        // next test underscore index exts
        for(auto ext : exts) {
          rel_path = join_paths(base, join_paths(name, "_index" + ext));
          abs_path = join_paths(root, rel_path);
          if (file_exists(abs_path, CWD)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
        }
        // next test plain index exts
        for(auto ext : exts) {
          rel_path = join_paths(base, join_paths(name, "index" + ext));
          abs_path = join_paths(root, rel_path);
          if (file_exists(abs_path, CWD)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
        }
      }
      // nothing found
      return includes;
    }

    sass::vector<sass::string> find_files(const sass::string& file, const sass::string& CWD, const sass::vector<sass::string> paths)
    {
      sass::vector<sass::string> includes;
      for (sass::string path : paths) {
        sass::string abs_path(join_paths(path, file));
        if (file_exists(abs_path, CWD)) includes.emplace_back(abs_path);
      }
      return includes;
    }

    sass::vector<sass::string> find_files(const sass::string& file, const sass::string& CWD, struct Sass_Compiler* compiler)
    {
      // get the last import entry to get current base directory
      // struct Sass_Options* options = sass_compiler_get_options(compiler);
      Sass_Import_Entry import = sass_compiler_get_last_import(compiler);
      const sass::vector<sass::string>& incs = compiler->cpp_ctx->include_paths;
      // create the vector with paths to lookup
      sass::vector<sass::string> paths(1 + incs.size());
      paths.emplace_back(dir_name(import->abs_path));
      paths.insert(paths.end(), incs.begin(), incs.end());
      // dispatch to find files in paths
      return find_files(file, CWD, paths);
    }

    // helper function to search one file in all include paths
    // this is normally not used internally by libsass (C-API sugar)
    sass::string find_file(const sass::string& file, const sass::string& CWD, const sass::vector<sass::string> paths)
    {
      if (file.empty()) return file;
      auto res = find_files(file, CWD, paths);
      return res.empty() ? "" : res.front();
    }

    // helper function to resolve a filename
    sass::string find_include(const sass::string& file, const sass::string& CWD, const sass::vector<sass::string> paths)
    {
      // search in every include path for a match
      for (size_t i = 0, S = paths.size(); i < S; ++i)
      {
        sass::vector<Include> resolved(resolve_includes(paths[i], file, CWD));
        if (resolved.size()) return resolved[0].abs_path;
      }
      // nothing found
      return sass::string("");
    }

    // try to load the given filename
    // returned memory must be freed
    // will auto convert .sass files
    char* read_file(const sass::string& path, const sass::string& CWD)
    {
      #ifdef _WIN32
        BYTE* pBuffer;
        DWORD dwBytes;
        wchar_t resolved[32768];
        // windows unicode filepaths are encoded in utf16
        sass::string abspath(join_paths(CWD, path));
        if (!(abspath[0] == '/' && abspath[1] == '/')) {
          abspath = "//?/" + abspath;
        }
        std::wstring wpath(UTF_8::convert_to_utf16(abspath));
        std::replace(wpath.begin(), wpath.end(), '/', '\\');
        DWORD rv = GetFullPathNameW(wpath.c_str(), 32767, resolved, NULL);
        if (rv > 32767) throw Exception::OperationError("Path is too long");
        if (rv == 0) throw Exception::OperationError("Path could not be resolved");
        HANDLE hFile = CreateFileW(resolved, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        DWORD dwFileLength = GetFileSize(hFile, NULL);
        if (dwFileLength == INVALID_FILE_SIZE) return 0;
        // allocate an extra byte for the null char
        // and another one for edge-cases in lexer
        pBuffer = (BYTE*)malloc((dwFileLength+2)*sizeof(BYTE));
        ReadFile(hFile, pBuffer, dwFileLength, &dwBytes, NULL);
        pBuffer[dwFileLength+0] = '\0';
        pBuffer[dwFileLength+1] = '\0';
        CloseHandle(hFile);
        // just convert from unsigned char*
        char* contents = (char*) pBuffer;
      #else
        // Read the file using `<cstdio>` instead of `<fstream>` for better portability.
        // The `<fstream>` header initializes `<locale>` and this buggy in GCC4/5 with static linking.
        // See:
        // https://www.spinics.net/lists/gcchelp/msg46851.html
        // https://github.com/sass/sassc-ruby/issues/128
        struct stat st;
        if (stat(path.c_str(), &st) == -1 || S_ISDIR(st.st_mode)) return 0;
        FILE* fd = std::fopen(path.c_str(), "rb");
        if (fd == nullptr) return nullptr;
        const std::size_t size = st.st_size;
        char* contents = static_cast<char*>(malloc(st.st_size + 2 * sizeof(char)));
        if (std::fread(static_cast<void*>(contents), 1, size, fd) != size) {
          free(contents);
          std::fclose(fd);
          return nullptr;
        }
        if (std::fclose(fd) != 0) {
          free(contents);
          return nullptr;
        }
        contents[size] = '\0';
        contents[size + 1] = '\0';
      #endif
      return contents;
    }

    // split a path string delimited by semicolons or colons (OS dependent)
    // sass::vector<sass::string> split_path_list(sass::string str)
    // {
    //   return Util::split_string(str, PATH_SEP);
    // }

  }
}
