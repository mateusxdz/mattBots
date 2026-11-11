#pragma once
#include <Arduino.h>
#include <vector>

namespace TextUtils {

  // Remove pontuação e espaços das extremidades (equivalente a sanitize_voice_text)
  inline String sanitizeText(const String& input) {
    String s = input;
    s.trim();
    while (s.length() > 0) {
      char first = s.charAt(0);
      if (first == '"' || first == '\'' || first == '`' || first == ' ' || first == ',' || first == ':' || first == ';') {
        s.remove(0, 1);
      } else {
        break;
      }
    }
    while (s.length() > 0) {
      char last = s.charAt(s.length() - 1);
      if (last == '"' || last == '\'' || last == '`' || last == '.' || last == ',' ||
          last == '!' || last == '?' || last == ':' || last == ';' || last == ' ' || last == '-') {
        s.remove(s.length() - 1, 1);
      } else {
        break;
      }
    }
    return s;
  }

  // Converte caracteres ASCII para minúsculas e normaliza acentos comuns em UTF-8 para facilitar matching
  inline String normalizeLower(const String& input) {
    String s = sanitizeText(input);
    s.toLowerCase();
    s.replace("á", "a");
    s.replace("à", "a");
    s.replace("ã", "a");
    s.replace("â", "a");
    s.replace("Á", "a");
    s.replace("Ã", "a");
    s.replace("é", "e");
    s.replace("ê", "e");
    s.replace("É", "e");
    s.replace("Ê", "e");
    s.replace("í", "i");
    s.replace("Í", "i");
    s.replace("ó", "o");
    s.replace("õ", "o");
    s.replace("ô", "o");
    s.replace("Ó", "o");
    s.replace("Õ", "o");
    s.replace("ú", "u");
    s.replace("Ú", "u");
    s.replace("ç", "c");
    s.replace("Ç", "c");
    return s;
  }

  // Capitaliza a primeira letra
  inline String capitalizeFirst(const String& input) {
    String s = input;
    s.trim();
    if (s.length() == 0) {
      return s;
    }
    char c = s.charAt(0);
    if (c >= 'a' && c <= 'z') {
      s.setCharAt(0, c - ('a' - 'A'));
    }
    return s;
  }

  // Divide argumentos separados por espaço
  inline std::vector<String> splitArgs(const String& text) {
    std::vector<String> tokens;
    int len = text.length();
    int i = 0;
    while (i < len) {
      while (i < len && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
        i++;
      }
      if (i >= len) break;
      int start = i;
      while (i < len && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n') {
        i++;
      }
      tokens.push_back(text.substring(start, i));
    }
    return tokens;
  }

  // Extrai comando (sem @botname) e texto de argumentos após o comando
  inline bool parseSlashCommand(const String& rawText, String& outCommand, String& outRest, std::vector<String>& outArgs) {
    String trimmed = rawText;
    trimmed.trim();
    if (!trimmed.startsWith("/")) {
      return false;
    }

    int firstSpace = -1;
    for (unsigned int i = 0; i < trimmed.length(); i++) {
      if (trimmed[i] == ' ' || trimmed[i] == '\n' || trimmed[i] == '\t') {
        firstSpace = i;
        break;
      }
    }

    String cmdToken = (firstSpace == -1) ? trimmed.substring(1) : trimmed.substring(1, firstSpace);
    int atIdx = cmdToken.indexOf('@');
    if (atIdx != -1) {
      cmdToken = cmdToken.substring(0, atIdx);
    }
    cmdToken.toLowerCase();
    outCommand = cmdToken;

    if (firstSpace != -1) {
      outRest = trimmed.substring(firstSpace + 1);
      outRest.trim();
      outArgs = splitArgs(outRest);
    } else {
      outRest = "";
      outArgs.clear();
    }
    return true;
  }

  // Remove pontuação/conectivos iniciais ("de", "do", "da", "com", "em", "no", "na", ":", "-")
  inline String stripLeadingConnectors(const String& input) {
    String s = input;
    s.trim();
    while (s.length() > 0 && (s[0] == ',' || s[0] == ':' || s[0] == '-' || s[0] == '.' || s[0] == ' ')) {
      s.remove(0, 1);
    }
    const char* prefixes[] = {
      "no mercado ", "na lista de compras ", "na lista ", "ao mercado ",
      "item no mercado ", "com o ", "com a ", "para ", "pra ",
      "de ", "do ", "da ", "com ", "em ", "no ", "na "
    };
    String lower = s;
    lower.toLowerCase();
    for (const char* p : prefixes) {
      size_t plen = strlen(p);
      if (lower.startsWith(p)) {
        s = s.substring(plen);
        s.trim();
        break;
      }
    }
    while (s.length() > 0 && (s[0] == ',' || s[0] == ':' || s[0] == '-' || s[0] == '.' || s[0] == ' ')) {
      s.remove(0, 1);
    }
    return s;
  }

} // namespace TextUtils

