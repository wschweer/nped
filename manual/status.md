## Current Status

In its current version, NPed as a mini-IDE serves only my personal preferences and workflow, and is not easily adaptable to other environments. To achieve this, the configuration options would need to be significantly expanded.

But hey, this is a C++ IDE, so you are a programmer and can modify the code directly :-)

The Details:

- Editor

  NPed is developed using NPed; the editor part is almost complete and (for me) "feature-complete".

- Language Server

  - The implementation is not entirely complete,
  - likely still contains bugs,
  - and the integration into the workflow is partially experimental.

- Code Formatting

  Code formatting uses `clang-format`.
  My preferred code style is xxx (I forgot the name). Unfortunately, `clang-format` does not support it exactly. The current hack uses a special `clang-format` configuration file with a small post-processor in the editor. This is why a simple swap of the `clang` configuration will not yield the desired result. For that, the post-processor hack must also be removed from the editor.

- AI Integration

  I use Claude (Anthropic), Gemini (Google), and local LLMs via Ollama for the development of this editor as well as for other projects. OpenAI is untested. Techniques for shortening the chat history (the context) are essential to reduce token consumption (and thus costs) and still need to be further refined.

  Sometimes the AI gets stuck in a loop that currently can only be broken by terminating the program. Further code is required here to detect and handle such situations.
