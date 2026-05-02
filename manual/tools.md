# Tools Manual

| Tool Name | Description |
|-----------|-------------|
| format_source | Formats a source file using the Language Server. |
| search_project | Searches for a text query across all files in the project, including unsaved editor buffers. |
| find_symbol | Uses the Language Server (LSP) to find the definition of a symbol like a class or function. |
| get_file_outline | Uses the Language Server (LSP) to get a hierarchical outline of classes, methods, and functions in a file. |
| get_diagnostics | Retrieve Language Server diagnostics (errors, warnings) for a file. |
| create_or_update_file | Create or update a single file in a GitHub repository |
| search_repositories | Search for GitHub repositories |
| create_repository | Create a new GitHub repository in your account |
| get_file_contents | Get the contents of a file or directory from a GitHub repository |
| push_files | Push multiple files to a GitHub repository in a single commit |
| create_issue | Create a new issue in a GitHub repository |
| create_pull_request | Create a new pull request in a GitHub repository |
| fork_repository | Fork a GitHub repository to your account or specified organization |
| create_branch | Create a new branch in a GitHub repository |
| list_commits | Get list of commits of a branch in a GitHub repository |
| list_issues | List issues in a GitHub repository with filtering options |
| update_issue | Update an existing issue in a GitHub repository |
| add_issue_comment | Add a comment to an existing issue |
| search_code | Search for code across GitHub repositories |
| search_issues | Search for issues and pull requests across GitHub repositories |
| search_users | Search for users on GitHub |
| get_issue | Get details of a specific issue in a GitHub repository. |
| get_pull_request | Get details of a specific pull request |
| list_pull_requests | List and filter repository pull requests |
| create_pull_request_review | Create a review on a pull request |
| merge_pull_request | Merge a pull request |
| get_pull_request_files | Get the list of files changed in a pull request |
| get_pull_request_status | Get the combined status of all status checks for a pull request |
| update_pull_request_branch | Update a pull request branch with the latest changes from the base branch |
| get_pull_request_comments | Get the review comments on a pull request |
| get_pull_request_reviews | Get the reviews on a pull request |
| web_search | Search the web for information.\nUse this tool when you need to search the web for information.\nYou can use this tool to search for news, blogs, or all types of information.\nYou can also use this tool to search for information about a specific company or product.\nYou can also use this tool to search for information about a specific person.\nYou can also use this tool to search for information about a specific product.\nYou can also use this tool to search for information about a specific company.\nYou can also use this tool to search for information about a specific event.\nYou can also use this tool to search for information about a specific location.\nYou can also use this tool to search for information about a specific thing.\nIf you request search with 1 result number and failed, retry with bigger results number. |
| read_text_file | Read the complete contents of a text file from the file system as text. Handles various text encodings and provides detailed error messages if the file cannot be read. Use this tool when you need to examine the contents of a single file. Optionally include line numbers for precise code targeting. Only works within allowed directories. |
| create_directory | Create a new directory or ensure a directory exists. Can create multiple nested directories in one operation. If the directory already exists, this operation will succeed silently. Perfect for setting up directory structures for projects or ensuring required paths exist. Only works within allowed directories. |
| directory_tree | Get a recursive tree view of files and directories as a JSON structure. Each entry includes 'name', 'type' (file/directory), and 'children' for directories. Files have no children array, while directories always have a children array (which may be empty). If the 'max_depth' parameter is provided, the traversal will be limited to the specified depth. As a result, the returned directory structure may be incomplete or provide a skewed representation of the full directory tree, since deeper-level files and subdirectories beyond the specified depth will be excluded. The output is formatted with 2-space indentation for readability. Only works within allowed directories. |
| get_file_info | Retrieve detailed metadata about a file or directory. Returns comprehensive information including size, creation time, last modified time, permissions, and type. This tool is perfect for understanding file characteristics without reading the actual content. Only works within allowed directories. |
| list_allowed_directories | Returns a list of directories that the server has permission to access Subdirectories within these allowed directories are also accessible. Use this to identify which directories and their nested paths are available before attempting to access files. |
| list_directory | Get a detailed listing of all files and directories in a specified path. Results clearly distinguish between files and directories with [FILE] and [DIR] prefixes. This tool is essential for understanding directory structure and finding specific files within a directory. Only works within allowed directories. |
| move_file | Move or rename files and directories. Can move files between directories and rename them in a single operation. If the destination exists, the operation will fail. Works across different directories and can be used for simple renaming within the same directory. Both source and destination must be within allowed directories. |
| read_multiple_text_files | Read the contents of multiple text files simultaneously as text. This is more efficient than reading files one by one when you need to analyze or compare multiple files. Each file's content is returned with its path as a reference. Failed reads for individual files won't stop the entire operation. Only works within allowed directories. |
| search_files | Recursively search for files and directories matching a pattern. Searches through all subdirectories from the starting path. The search is case-insensitive and matches partial names. Returns full paths to all matching items.Optional 'min_bytes' and 'max_bytes' arguments can be used to filter files by size, ensuring that only files within the specified byte range are included in the search. This tool is great for finding files when you don't know their exact location or find files by their size.Only searches within allowed directories. |
| write_file | Create a new file or completely overwrite an existing file with new content. Use with caution as it will overwrite existing files without warning. Handles text content with proper encoding. Only works within allowed directories. |
| zip_files | Creates a ZIP archive by compressing files. It takes a list of files to compress and a target path for the resulting ZIP file. Both the source files and the target ZIP file should reside within allowed directories. |
| unzip_file | Extracts the contents of a ZIP archive to a specified target directory.\nIt takes a source ZIP file path and a target extraction directory.\nThe tool decompresses all files and directories stored in the ZIP, recreating their structure in the target location.\nBoth the source ZIP file and the target directory should reside within allowed directories. |
| zip_directory | Creates a ZIP archive by compressing a directory , including files and subdirectories matching a specified glob pattern.\nIt takes a path to the folder and a glob pattern to identify files to compress and a target path for the resulting ZIP file.\nBoth the source directory and the target ZIP file should reside within allowed directories. |
| search_files_content | Searches for text or regex patterns in the content of files matching matching a GLOB pattern.Returns detailed matches with file path, line number, column number and a preview of matched text.By default, it performs a literal text search; if the 'is_regex' parameter is set to true, it performs a regular expression (regex) search instead.Optional 'min_bytes' and 'max_bytes' arguments can be used to filter files by size, ensuring that only files within the specified byte range are included in the search. Ideal for finding specific code, comments, or text when you don't know their exact location. |
| list_directory_with_sizes | Get a detailed listing of all files and directories in a specified path, including sizes. Results clearly distinguish between files and directories with [FILE] and [DIR] prefixes. This tool is useful for understanding directory structure and finding specific files within a directory. Only works within allowed directories. |
| read_media_file | Reads an image or audio file and returns its Base64-encoded content along with the corresponding MIME type. The max_bytes argument could be used to enforce an upper limit on the size of a file to read if the media file exceeds this limit, the operation will return an error instead of reading the media file. Access is restricted to files within allowed directories only. |
| read_multiple_media_files | Reads multiple image or audio files and returns their Base64-encoded contents along with corresponding MIME types. This method is more efficient than reading files individually. The max_bytes argument could be used to enforce an upper limit on the size of a file to read Failed reads for specific files are skipped without interrupting the entire operation. Only works within allowed directories. |
| head_file | Reads and returns the first N lines of a text file.This is useful for quickly previewing file contents without loading the entire file into memory.If the file has fewer than N lines, the entire file will be returned.Only works within allowed directories. |
| tail_file | Reads and returns the last N lines of a text file.This is useful for quickly previewing file contents without loading the entire file into memory.If the file has fewer than N lines, the entire file will be returned.Only works within allowed directories. |
| read_file_lines | Reads lines from a text file starting at a specified line offset (0-based) and continues for the specified number of lines if a limit is provided.This function skips the first 'offset' lines and then reads up to 'limit' lines if specified, or reads until the end of the file otherwise.It's useful for partial reads, pagination, or previewing sections of large text files.Only works within allowed directories. |
| find_empty_directories | Recursively finds all empty directories within the given root path.A directory is considered empty if it contains no files in itself or any of its subdirectories.Operating system metadata files `.DS_Store` (macOS) and `Thumbs.db` (Windows) will be ignored.The optional exclude_patterns argument accepts glob-style patterns to exclude specific paths from the search.Only works within allowed directories. |
| calculate_directory_size | Calculates the total size of a directory specified by `root_path`.It recursively searches for files and sums their sizes. The result can be returned in either a `human-readable` format or as `bytes`, depending on the specified `output_format` argument.Only works within allowed directories. |
| find_duplicate_files | Find duplicate files within a directory and return list of duplicated files as text or json formatOptional `pattern` argument can be used to narrow down the file search to specific glob pattern.Optional `exclude_patterns` can be used to exclude certain files matching a glob.`min_bytes` and `max_bytes` are optional arguments that can be used to restrict the search to files with sizes within a specified range.The output_format argument specifies the format of the output and accepts either `text` or `json` (default: text).Only works within allowed directories. |
| git_status | Shows the working tree status |
| git_diff_unstaged | Shows changes in the working directory that are not yet staged |
| git_diff_staged | Shows changes that are staged for commit |
| git_diff | Shows differences between branches or commits |
| git_commit | Records changes to the repository |
| git_add | Adds file contents to the staging area |
| git_reset | Unstages all staged changes |
| git_log | Shows the commit log |
| git_create_branch | Creates a new branch from an optional base branch |
| git_checkout | Switches branches |
| git_show | Shows the contents of a commit |
| git_branch | List Git branches |
| run_valgrind | Executes a compiled C/C++ program under Valgrind and returns a compressed error report. |
| bash_command | Executes a shell command. Use this for general bash tasks. |
| build_project | Builds the project. An optional parameter can be given to build a specific target. |
