// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

// Some functions are heavily inspired by perl
// https://perldoc.perl.org/File/Basename.html
// https://perldoc.perl.org/File/Spec.html

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
#include "character.hpp"
#include "unicode.hpp"
#include "sass_functions.hpp"
#include "error_handling.hpp"
#include "util.hpp"
#include "util_string.hpp"

#ifdef _WIN32
# include <windows.h>

# ifdef _MSC_VER
# include <codecvt>
inline static Sass::sass::string wstring_to_string(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wchar_converter;
    return wchar_converter.to_bytes(wstr).c_str();
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
    extern sass::string get_cwd()
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
        sass::string cwd = wstring_to_string(pwd);
        //convert backslashes to forward slashes
        replace(cwd.begin(), cwd.end(), '\\', '/');
      #endif
      if (cwd[cwd.length() - 1] != '/') cwd += '/';
      return cwd;
    }


    // test if path exists and is a file
    // takes a cache map to improve performance
    bool file_exists(const sass::string& path, const sass::string& CWD, std::unordered_map<sass::string, bool>& cache)
    {
      #ifdef _WIN32
        wchar_t resolved[32768];
        // windows unicode file-paths are encoded in utf16
        sass::string abspath(join_paths(CWD, path));
        if (!(abspath[0] == '/' && abspath[1] == '/')) {
          abspath = "//?/" + abspath;
        }
        auto it = cache.find(abspath);
        if (it != cache.end()) {
          return it->second;
        }
        sass::wstring wpath(Unicode::utf8to16(abspath));
        std::replace(wpath.begin(), wpath.end(), '/', '\\');
        DWORD rv = GetFullPathNameW(wpath.c_str(), 32767, resolved, NULL);
        if (rv > 32767) throw Exception::OperationError("Path is too long");
        if (rv == 0) throw Exception::OperationError("Path could not be resolved");
        DWORD dwAttrib = GetFileAttributesW(resolved); // was 3%
        bool result = (dwAttrib != INVALID_FILE_ATTRIBUTES
          && (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)));
        cache[abspath] = result;
        return result;
      #else
        struct stat st_buf;
        sass::string abspath(join_paths(CWD, path));
        bool result = (stat (abspath.c_str(), &st_buf) == 0)
          && (!S_ISDIR (st_buf.st_mode));
        cache[abspath] = result;
        return result;
      #endif
    }

    // return if given path is absolute
    // works with *nix and windows paths
    bool is_absolute_path(const sass::string& path)
    {
      #ifdef _WIN32
        if (path.length() >= 3 && Character::isAlphabetic(path[0]) && path[1] == ':') return true;
      #endif
      size_t i = 0;
      // check if we have a protocol
      if (path[i] && Character::isAlphabetic(static_cast<unsigned char>(path[i]))) {
        // skip over all alphanumeric characters
        while (path[i] && Character::isAlphanumeric(static_cast<unsigned char>(path[i]))) ++i;
        i = i && path[i] == ':' ? i + 1 : 0;
      }
      return path[i] == '/';
    }
    // EO is_absolute_path

    // helper function to find the last directory separator
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
    // EO find_last_folder_separator

    // return only the directory part of path
    sass::string dir_name(const sass::string& path)
    {
      size_t pos = find_last_folder_separator(path);
      if (pos == sass::string::npos) return "";
      else return path.substr(0, pos+1);
    }
    // EO dir_name

    // return only the filename part of path
    sass::string base_name(const sass::string& path)
    {
      size_t pos = find_last_folder_separator(path);
      if (pos == sass::string::npos) return path;
      else return path.substr(pos+1);
    }
    // EO base_name

    // do a logical clean up of the path
    // no physical check on the file-system
    sass::string make_canonical_path (sass::string path)
    {

      // declarations
      size_t pos;

      #ifdef _WIN32
        //convert backslashes to forward slashes
        replace(path.begin(), path.end(), '\\', '/');
      #endif

      pos = 0; // remove all self references inside the path string (`/./`)
      while((pos = path.find("/./", pos)) != sass::string::npos) path.erase(pos, 2);

      // remove all leading and trailing self references
      while(path.size() >= 2 && path[0] == '.' && path[1] == '/') path.erase(0, 2);
      while((pos = path.length()) > 1 && path[pos - 2] == '/' && path[pos - 1] == '.') path.erase(pos - 2);

      size_t proto = 0;
      // check if we have a protocol
      if (path[proto] && Character::isAlphabetic(path[proto])) {
        // skip over all alphanumeric characters
        while (path[proto] && Character::isAlphanumeric(path[proto++])) {}
        // then skip over the mandatory colon
        if (proto && path[proto] == ':') ++ proto;
      }

      // then skip over start slashes
      while (path[proto++] == '/') {}

      pos = proto; // collapse multiple delimiters into a single one
      while((pos = path.find("//", pos)) != sass::string::npos) path.erase(pos, 1);

      return path;

    }
    // EO make_canonical_path

    // join two path segments cleanly together
    // but only if right side is not absolute yet
    // Can we avoid the two string copies?
    // OK, one copy is needed anyway
    sass::string join_paths(sass::string l, sass::string r)
    {

      #ifdef _WIN32
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
    // EO join_paths

    sass::string rel2dbg(const sass::string& rel_path, const sass::string& orig_path)
    {
      // if the file is outside this directory show the absolute path
      return rel_path.substr(0, 3) == "../" ? orig_path : rel_path;
    }
    // EO rel2dbg

    // create an absolute path by resolving relative paths with cwd
    sass::string rel2abs(const sass::string& path, const sass::string& base)
    {
      return rel2abs(path, base, Sass::CWD);
    }
    // EO rel2abs

    // create an absolute path by resolving relative paths with cwd
    sass::string rel2abs(const sass::string& path, const sass::string& base, const sass::string& CWD)
    {
      return make_canonical_path(join_paths(join_paths(CWD + "/", base + "/"), path));
    }
    // EO rel2abs


    // create a path that is relative to the given base directory
    // path and base will first be resolved against cwd to make them absolute
    sass::string abs2rel(const sass::string& path, const sass::string& base)
    {
      return abs2rel(path, base, Sass::CWD);
    }
    // EO abs2rel

    // create a path that is relative to the given base directory
    // path and base will first be resolved against cwd to make them absolute
    sass::string abs2rel(const sass::string& path, const sass::string& base, const sass::string& CWD)
    {

      sass::string abs_path = rel2abs(path, CWD, CWD);
      sass::string abs_base = rel2abs(base, CWD, CWD);

      size_t proto = 0;
      // check if we have a protocol
      if (path[proto] && Character::isAlphabetic(static_cast<unsigned char>(path[proto]))) {
        // skip over all alphanumeric characters
        while (path[proto] && Character::isAlphanumeric(static_cast<unsigned char>(path[proto++]))) {}
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
          // compare the characters in a case insensitive manner
          // windows FS is only case insensitive in ASCII ranges
          if (!Character::characterEqualsIgnoreCase(abs_path[i], abs_base[i])) break;
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
    // EO abs2rel

    // Resolution order for ambiguous imports:
    // (1) filename as given
    // (2) underscore + given
    // (3) underscore + given + extension
    // (4) given + extension
    // (5) given + _index.scss
    // (6) given + _index.sass
    sass::vector<Include> resolve_includes(
      const sass::string& root,
      const sass::string& file,
      const sass::string& CWD,
      std::unordered_map<sass::string, bool>& cache,
      const std::vector<sass::string>& exts)
    {
      sass::string filename = join_paths(root, file);
      // split the filename
      sass::string base(dir_name(file));
      sass::string name(base_name(file));
      sass::vector<Include> includes;
      // create full path (maybe relative)
      sass::string rel_path(join_paths(base, name));
      sass::string abs_path(join_paths(root, rel_path));
      if (file_exists(abs_path, CWD, cache)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      // next test variation with underscore
      rel_path = join_paths(base, "_" + name);
      abs_path = join_paths(root, rel_path);
      if (file_exists(abs_path, CWD, cache)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      // next test extensions plus underscore
      for(auto ext : exts) {
        rel_path = join_paths(base, "_" + name + ext);
        abs_path = join_paths(root, rel_path);
        if (file_exists(abs_path, CWD, cache)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      }
      // next test plain name with extensions
      for(auto ext : exts) {
        rel_path = join_paths(base, name + ext);
        abs_path = join_paths(root, rel_path);
        if (file_exists(abs_path, CWD, cache)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
      }
      // index files
      if (includes.size() == 0) {
        // ignore directories that look like @import'able filename
        for(auto ext : exts) {
          if (ends_with(name, ext)) return includes;
        }
        // next test underscore index extensions
        for(auto ext : exts) {
          rel_path = join_paths(base, join_paths(name, "_index" + ext));
          abs_path = join_paths(root, rel_path);
          if (file_exists(abs_path, CWD, cache)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
        }
        // next test plain index extensions
        for(auto ext : exts) {
          rel_path = join_paths(base, join_paths(name, "index" + ext));
          abs_path = join_paths(root, rel_path);
          if (file_exists(abs_path, CWD, cache)) includes.push_back({{ rel_path, root }, abs_path, SASS_IMPORT_AUTO });
        }
      }
      // nothing found
      return includes;
    }
    // EO resolve_includes

    // Private helper function for find_file
    sass::vector<sass::string> _find_file(const sass::string& file, const sass::string& CWD, const sass::vector<sass::string> paths, std::unordered_map<sass::string, bool>& cache)
    {
      sass::vector<sass::string> includes;
      for (sass::string path : paths) {
        sass::string abs_path(join_paths(path, file));
        if (file_exists(abs_path, CWD, cache)) includes.emplace_back(abs_path);
      }
      return includes;
    }
    // EO find_files

    // helper function to search one file in all include paths
    // this is normally not used internally by libsass (C-API sugar)
    sass::string find_file(const sass::string& file, const sass::string& CWD, const sass::vector<sass::string> paths, std::unordered_map<sass::string, bool>& cache)
    {
      if (file.empty()) return file;
      auto res = _find_file(file, CWD, paths, cache);
      return res.empty() ? "" : res.front();
    }

    // helper function to resolve a filename
    sass::string find_include(const sass::string& file, const sass::string& CWD, const sass::vector<sass::string> paths, std::unordered_map<sass::string, bool>& cache)
    {
      // search in every include path for a match
      for (size_t i = 0, S = paths.size(); i < S; ++i)
      {
        sass::vector<Include> resolved(resolve_includes(paths[i], file, CWD, cache));
        if (resolved.size()) return resolved[0].abs_path;
      }
      // nothing found
      return sass::string("");
    }

    // try to load the given filename
    // returned memory must be freed
    // will auto convert .sass files
    char* slurp_file(const sass::string& path, const sass::string& CWD)
    {
      #ifdef _WIN32
        char* contents;
        DWORD dwBytes;
        wchar_t resolved[32768];
        // windows unicode file-paths are encoded in utf16
        sass::string abspath(join_paths(CWD, path));
        if (!(abspath[0] == '/' && abspath[1] == '/')) {
          abspath = "//?/" + abspath;
        }
        sass::wstring wpath(Unicode::utf8to16(abspath));
        std::replace(wpath.begin(), wpath.end(), '/', '\\');
        DWORD rv = GetFullPathNameW(wpath.c_str(), 32767, resolved, NULL);
        if (rv > 32767) throw Exception::OperationError("Path is too long");
        if (rv == 0) throw Exception::OperationError("Path could not be resolved");
        HANDLE hFile = CreateFileW(resolved, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        DWORD dwFileLength = GetFileSize(hFile, NULL);
        if (dwFileLength == INVALID_FILE_SIZE) return 0;
        // allocate an extra byte for the null terminator
        contents = (char*) sass_alloc_memory(size_t(dwFileLength) + 1);
        if (!ReadFile(hFile, contents, dwFileLength, &dwBytes, NULL))
          throw Exception::OperationError("Could not read file");
        contents[dwFileLength] = '\0'; // ensure null terminator
        CloseHandle(hFile);
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
        char* contents = static_cast<char*>(sass_alloc_memory(st.st_size + 1 * sizeof(char)));
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
      #endif
      return contents;
    }
    // EO slurp_file

  }
}
