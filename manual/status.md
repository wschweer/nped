## Current Status

In its current version, NPed as a mini-IDE serves only my personal preferences and workflow, and is not easily adaptable to other environments. To achieve this, the configuration options would need to be significantly expanded.

But hey, this is a C++ IDE, so you are a programmer and can modify the code directly :-)

The Details:

- Editor

  NPed is developed using NPed; the editor part is almost complete and (for me) "feature-complete".

- Language Server
  - The implementation is not complete,
  - likely still contains bugs,
  - and the integration into the workflow is partially experimental.

- Code Formatting

  Code formatting uses `clang-format`.
  My preferred code style is Ratliff/Banner-Stile. Unfortunately, `clang-format` does not support
  it exactly. The current hack uses a special `clang-format` configuration file with a small
  post-processor in the editor.
  This is why a simple swap of the `clang` configuration will not yield the desired result.
  For that, the post-processor hack must also be removed from the editor.

- AI Integration

  I am using mainly Ollama. Claude (Anthropic), Gemini (Google) and OpenAI are not tested very well.
  NPed is used to develop NPed itself and some other projects.
