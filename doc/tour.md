# A Tour of Ceramic

Ceramic is a systems language built for generic programming. This tour covers enough so you can start writing Ceramic code and follow the [Language Reference](language-reference/index.md). It assumes prior programming experience in any language. Most examples are complete programs.

### Hello, World!

The smallest Ceramic program is a `main` function:

```ceramic
import printer.(println);

main() {
  println("Hello, World!");
}
```

Save it as `hello.crm` and compile it to an executable:

```
ceramic hello.crm
./hello
```

Using `-run` skips the binary and runs immediately:

```sh
ceramic -run hello.crm
```

