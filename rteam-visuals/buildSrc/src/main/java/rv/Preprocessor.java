package rv;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Deque;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Turns the shared multi-version sources into sources for one Minecraft version.
 *
 * <pre>
 * //#if MC >= 1.20
 * code for 1.20 and newer
 * //#elseif MC >= 1.17
 * code for 1.17 .. 1.19.x
 * //#else
 * code for older versions
 * //#endif
 * </pre>
 *
 * Inactive lines become empty lines so compiler errors keep their line numbers.
 * Active lines may be written as {@code //$$ code} and are uncommented.
 * Variables: MC (1.20.1 -> 12001, 26.1 -> 260100), JAVA (8, 16, 17, 21, 25), OBF (1 up to 1.21.11).
 */
public final class Preprocessor {
    private final Map<String, Integer> vars;

    public Preprocessor(Map<String, Integer> vars) {
        this.vars = vars;
    }

    public static int parseVersion(String version) {
        String[] parts = version.trim().split("\\.");
        int major = Integer.parseInt(parts[0]);
        int minor = parts.length > 1 ? Integer.parseInt(parts[1]) : 0;
        int patch = parts.length > 2 ? Integer.parseInt(parts[2]) : 0;
        return major * 10000 + minor * 100 + patch;
    }

    /** Processes a source tree. Files listed in {@code templated} also get ${key} replacement. */
    public void processTree(Path src, Path dst, Map<String, String> templateValues) {
        if (!Files.isDirectory(src)) return;
        try (Stream<Path> walk = Files.walk(src)) {
            for (Path file : walk.filter(Files::isRegularFile).collect(Collectors.toList())) {
                Path rel = src.relativize(file);
                Path out = dst.resolve(rel.toString());
                String name = file.getFileName().toString();
                boolean text = name.endsWith(".java") || name.endsWith(".json") || name.endsWith(".lang") || name.endsWith(".txt");
                Files.createDirectories(out.getParent());
                if (!text) {
                    Files.copy(file, out, StandardCopyOption.REPLACE_EXISTING);
                    continue;
                }
                List<String> lines = Files.readAllLines(file, StandardCharsets.UTF_8);
                List<String> result = process(lines, rel.toString());
                boolean empty = true;
                for (String line : result) {
                    if (!line.trim().isEmpty()) {
                        empty = false;
                        break;
                    }
                }
                if (empty) continue;
                if (name.equals("fabric.mod.json")) {
                    List<String> replaced = new ArrayList<>(result.size());
                    for (String line : result) {
                        for (Map.Entry<String, String> e : templateValues.entrySet()) {
                            line = line.replace("${" + e.getKey() + "}", e.getValue());
                        }
                        replaced.add(line);
                    }
                    result = replaced;
                }
                Files.write(out, result, StandardCharsets.UTF_8);
            }
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    public List<String> process(List<String> lines, String fileName) {
        List<String> out = new ArrayList<>(lines.size());
        Deque<boolean[]> stack = new ArrayDeque<>(); // {parentActive, branchTaken}
        boolean active = true;
        for (int i = 0; i < lines.size(); i++) {
            String line = lines.get(i);
            String t = line.trim();
            int lineNo = i + 1;
            if (t.startsWith("//#if ")) {
                boolean cond = eval(t.substring(6), fileName, lineNo);
                stack.push(new boolean[]{active, cond});
                active = active && cond;
                out.add("");
            } else if (t.startsWith("//#elseif ")) {
                boolean[] frame = frame(stack, fileName, lineNo, "//#elseif");
                boolean cond = !frame[1] && eval(t.substring(10), fileName, lineNo);
                if (cond) frame[1] = true;
                active = frame[0] && cond;
                out.add("");
            } else if (t.equals("//#else")) {
                boolean[] frame = frame(stack, fileName, lineNo, "//#else");
                boolean cond = !frame[1];
                frame[1] = true;
                active = frame[0] && cond;
                out.add("");
            } else if (t.equals("//#endif")) {
                boolean[] frame = frame(stack, fileName, lineNo, "//#endif");
                stack.pop();
                active = frame[0];
                out.add("");
            } else if (t.startsWith("//#")) {
                throw new IllegalStateException(fileName + ":" + lineNo + ": unknown directive " + t);
            } else if (active) {
                int idx = line.indexOf("//$$");
                if (idx >= 0 && line.substring(0, idx).trim().isEmpty()) {
                    String rest = line.substring(idx + 4);
                    if (rest.startsWith(" ")) rest = rest.substring(1);
                    line = line.substring(0, idx) + rest;
                }
                out.add(line);
            } else {
                out.add("");
            }
        }
        if (!stack.isEmpty()) {
            throw new IllegalStateException(fileName + ": " + stack.size() + " unclosed //#if");
        }
        return out;
    }

    private static boolean[] frame(Deque<boolean[]> stack, String file, int line, String what) {
        boolean[] frame = stack.peek();
        if (frame == null) throw new IllegalStateException(file + ":" + line + ": " + what + " without //#if");
        return frame;
    }

    // ---- expression evaluation -------------------------------------------------------------

    public boolean eval(String expr, String file, int line) {
        try {
            Parser p = new Parser(tokenize(expr));
            int v = p.or();
            if (p.pos != p.tokens.size()) throw new IllegalStateException("unexpected " + p.tokens.get(p.pos));
            return v != 0;
        } catch (RuntimeException e) {
            throw new IllegalStateException(file + ":" + line + ": bad condition '" + expr + "': " + e.getMessage(), e);
        }
    }

    private static List<String> tokenize(String s) {
        List<String> tokens = new ArrayList<>();
        int i = 0;
        while (i < s.length()) {
            char c = s.charAt(i);
            if (Character.isWhitespace(c)) {
                i++;
            } else if (Character.isLetter(c) || c == '_') {
                int j = i;
                while (j < s.length() && (Character.isLetterOrDigit(s.charAt(j)) || s.charAt(j) == '_')) j++;
                tokens.add(s.substring(i, j));
                i = j;
            } else if (Character.isDigit(c)) {
                int j = i;
                while (j < s.length() && (Character.isDigit(s.charAt(j)) || s.charAt(j) == '.')) j++;
                tokens.add(s.substring(i, j));
                i = j;
            } else if (s.startsWith(">=", i) || s.startsWith("<=", i) || s.startsWith("==", i)
                    || s.startsWith("!=", i) || s.startsWith("&&", i) || s.startsWith("||", i)) {
                tokens.add(s.substring(i, i + 2));
                i += 2;
            } else if ("<>!()".indexOf(c) >= 0) {
                tokens.add(String.valueOf(c));
                i++;
            } else {
                throw new IllegalStateException("bad character '" + c + "'");
            }
        }
        return tokens;
    }

    private final class Parser {
        final List<String> tokens;
        int pos;

        Parser(List<String> tokens) {
            this.tokens = Collections.unmodifiableList(tokens);
        }

        String peek() {
            return pos < tokens.size() ? tokens.get(pos) : null;
        }

        String next() {
            if (pos >= tokens.size()) throw new IllegalStateException("unexpected end");
            return tokens.get(pos++);
        }

        int or() {
            int v = and();
            while ("||".equals(peek())) {
                next();
                int r = and();
                v = (v != 0 || r != 0) ? 1 : 0;
            }
            return v;
        }

        int and() {
            int v = unary();
            while ("&&".equals(peek())) {
                next();
                int r = unary();
                v = (v != 0 && r != 0) ? 1 : 0;
            }
            return v;
        }

        int unary() {
            if ("!".equals(peek())) {
                next();
                return unary() != 0 ? 0 : 1;
            }
            return comparison();
        }

        int comparison() {
            int left = operand();
            String op = peek();
            if (op == null) return left;
            int right;
            switch (op) {
                case ">=": next(); right = operand(); return left >= right ? 1 : 0;
                case "<=": next(); right = operand(); return left <= right ? 1 : 0;
                case ">": next(); right = operand(); return left > right ? 1 : 0;
                case "<": next(); right = operand(); return left < right ? 1 : 0;
                case "==": next(); right = operand(); return left == right ? 1 : 0;
                case "!=": next(); right = operand(); return left != right ? 1 : 0;
                default: return left;
            }
        }

        int operand() {
            String t = next();
            if (t.equals("(")) {
                int v = or();
                if (!")".equals(next())) throw new IllegalStateException("missing )");
                return v;
            }
            if (Character.isDigit(t.charAt(0))) {
                return t.indexOf('.') >= 0 ? parseVersion(t) : Integer.parseInt(t);
            }
            Integer v = vars.get(t);
            if (v == null) throw new IllegalStateException("unknown variable " + t);
            return v;
        }
    }

    /** Lists mixin classes (files containing an {@code @Mixin} annotation) below the given package directory. */
    public static List<String> findMixins(Path javaRoot, String mixinPackage) {
        Path dir = javaRoot.resolve(mixinPackage.replace('.', '/'));
        if (!Files.isDirectory(dir)) return Collections.emptyList();
        List<String> result = new ArrayList<>();
        try (Stream<Path> walk = Files.walk(dir)) {
            for (Path file : walk.filter(p -> p.toString().endsWith(".java")).sorted().collect(Collectors.toList())) {
                String content = new String(Files.readAllBytes(file), StandardCharsets.UTF_8);
                if (!content.contains("@Mixin(")) continue;
                String rel = dir.relativize(file).toString();
                result.add(rel.substring(0, rel.length() - 5).replace('/', '.').replace('\\', '.'));
            }
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
        return result;
    }

    public static void writeMixinConfig(Path file, String mixinPackage, int javaVersion, List<String> mixins) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\n");
        sb.append("  \"required\": true,\n");
        sb.append("  \"minVersion\": \"0.8\",\n");
        sb.append("  \"package\": \"").append(mixinPackage).append("\",\n");
        sb.append("  \"compatibilityLevel\": \"JAVA_").append(javaVersion).append("\",\n");
        sb.append("  \"client\": [\n");
        for (int i = 0; i < mixins.size(); i++) {
            sb.append("    \"").append(mixins.get(i)).append("\"");
            sb.append(i + 1 < mixins.size() ? ",\n" : "\n");
        }
        sb.append("  ],\n");
        sb.append("  \"injectors\": {\n    \"defaultRequire\": 0\n  }\n");
        sb.append("}\n");
        try {
            Files.createDirectories(file.getParent());
            Files.write(file, sb.toString().getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }
}
