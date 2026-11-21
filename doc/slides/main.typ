#import "@preview/typslides:1.4.0": *

#let codeblock(body, caption: none, lineNum:false) = {
  if lineNum {
    show raw.where(block:true): it =>{
      set text(size: 15pt)
      set par(justify: false)
      block(fill: luma(240),inset: 0.3em,radius: 0.3em,
        // grid size: N*2
        grid(
          columns: 2,
          align: left+top,
          column-gutter: 0.5em,
          stroke: (x,y) => if x==0 {( right: (paint:gray, dash:"densely-dashed") )},
          inset: 0.3em,
          ..it.lines.map((line) => (str(line.number), line.body)).flatten()
        )
      )
    }
    figure(body, caption: caption, kind: "code", supplement: "Code")
  }
  else{
    figure(body, caption: caption, kind: "code", supplement: "Code")
  }
}

#show: typslides.with(
  theme: "reddy",
)

// Dobrý den, moje jméno je Jakub Antonín Štigler a tohle je prezentace o mém
// projektu na téma
// Chytré reference pro ověřování Rustovských půjčovacích pravidel v C++.

#front-slide(
  title: "Smart references for checking Rust borrowing rules in C++",
  subtitle: "SCO project",
  authors: "Bc. Jakub Antonín Štigler"
)

// Nejdříve je nutné zmínit co jsou v Rustu půjčovací pravidla. Dají se
// rozdělit na proavidla o vlastnictví, které říkají že každá hodnota má právě
// jednoho vlastníka, který je zodpovědný za její uvolnění. Pokud by měl být
// vlastník uvolněn, tak i všechny hodnoty které vlastní jsou uvolněny. Tyto
// pravidla zaručují že je vždy zřejmé kdy se má paměť uvolnit aniž by to
// programátor musel explicitně psát a není tedy potřeba žádný garbage
// kolektor.
// Další skupinou jsou pravidla o referencích. První z nich říká, že na jednu
// hodnotu vždy ukazuje jen jedna mutable reference, nebo libovoný počet
// immutable referencí. Toto zamezuje vzniknutí data race, protože se nikdy
// nemůže stát že k paměti má někdo přístup ve chvíli kdy do ní někdo zapisuje.
// Druhé říká že reference jsou vždy validní. Každý přístup do paměti pak tedy
// také musí být validní.
// Pokud můžeme dokázat že program dodržuje tyto pravidla, můžeme i dokázat že
// některé typy chyb se v programu nevyskytují jako například use after free
// nebo data race.
// Pravidla o vlastnictví nejsou tak složitá a i C++ obsahuje prostředky pro
// jejich realizaci pomocí destruktorů. Obtížné je tedy ověřovat zejména
// pravidla o referencích.

#slide(title: "Borrowing rules in Rust")[
  #stress[Ownership rules]
  - Each value in Rust has an _owner_.
  - There can only be one owner at a time.
  - When the owner goes out of scope, the value will be dropped.
  #stress[Rules of references]
  - At any given time, you can have either one mutable reference or any number
    of immutable references.
  - References must always be valid.
]

// Rust řeší ověřování těchto pravidel při kompilaci. Pro ověření využívá
// takzvané lifetimy, které pro reference udržují informaci odkud pochází.
// Toto ověřování může být velmi složité a samotný Rust v tuhle chvíli není
// schopen toto korektně ověřit ve všech případech a je tedy unsound. V
// prezentaci je ukázka Rust kódu, který se zkompiluje ikdyž obsahuje use after
// free bez použití jakéhokoli unsafe kódu.

#slide(title: "Rust solution")[
  #cols(columns: (1fr, 2fr))[
    - Compile-time
    - Lifetimes
    - Difficult to realize
    - Currently unsound in edge cases
  ][
    #codeblock(lineNum: true)[
      ```rust
      struct Q<'a: 'static>(&'a str);
      trait Trait {
        fn wrong(&self) -> &'static str;
      }
      impl<'a> Trait for Q<'a> {
        fn wrong(&self) -> &'static str {
          self.0
        }
      }
      fn main() {
        let s = String::from("allocated");
        let mut q = Q("static");
        q.0 = &s;
        let sref = (&q as &dyn Trait).wrong();
        drop(l); // free s
        println!("{}", sref); // use after free
      }
      ```
    ]
  ]
]

// V rámci tohoto projektu jsem implementoval knihovnu borlib pro C++, která
// poskytuje chytré poitery pro ověřování půjčovacích pravidel za běhu
// programu. Ověřování za běhu je stále výhodné, protože porušení pravidel se
// dá detekovat dříve než nedefinované chování. Zde je ukázán příklad za
// použití borlibu. Dvě vlákna se snaží zapsat do stejného místa v paměti
// různou hodnotu což vede na data race. Borlib ale chybu ohlásí už při
// provádění řádku 7, tedy ještě dříve než jsou vlákna vůbec spuštěna. To
// protože na řídku 7 se druhá mutable reference a to porušuje půjčovací
// pravidla.
// Toto ověřování je realizováno atomickými čítači, které obsahuje každá
// instance `Val<T>`.

#slide(title: "Borlib in C++")[
  #cols(columns: (2fr, 3fr))[
    - Checking at runtime
    - Violation terminates process
    - Header-only library
    - Owning type `Val<T>`
    - Smart references `Ref<T>`, `RefMut<T>`, `Slice<T>` and `SliceMut<T>`
    - Each operation verifies rules
    - Helpful error messages
    - Realization with counters
  ][
    #codeblock(lineNum: true)[
      ```cpp
      void foo(RefMut<int> r, int v) {
        *r = v;
      }
      int main() {
        Val<int> v = 5;
        RefMut<int> r1 = v.ref_mut();
        RefMut<int> r2 = v.ref_mut();
        
        std::thread t1(foo, std::move(r1), 7);
        std::thread t2(foo, std::move(r2), 9);
        
        t1.join();
        t2.join();
      }
      ```
    ]
    #text(size: 15pt)[
      ```
      main.cpp:7:21: borrow violation: Cannot mutably borrow value that is already mutably borrowed.
      ```
    ]
  ]
]

// Zde mám další ukázku. Tentokrát se jedná o detekci use after free hodnoty
// `n` deklarované ve vnitřním bloku. Reference na hodnotu je uložena do
// proměnné, která ale tento blok přežije a tudíž by se po dokončení bloku
// jednalo o porušení půjčovacích pravidel. Borlib detekuje chybu na řádku `7`
// kde se implicitně volá destruktor hodnoty `n`.

#slide(title: "Example: use after free")[
  #codeblock(lineNum: true)[
    ```cpp
    int main() {
      Ref<int> ref;
      {
        Val<int> n = 5;
        ref = &n;
        std::println("pos1");
      }
      std::println("pos2");
      std::println("pos3: {}", ref);
    }
    ```
  ]
  #stress[Output:]
  ```
  pos1
  main.cpp:7:0: borrow violation: Cannot destroy value that is borrowed.
  ```
]

// V téhle další ukázce je využito featury které se říká reborrowing. Jedná se
// o znovuzapůjčení mutable reference. Po dobu toho zapůjčení není možné
// původní referenci používat. Znovuzapůjčení je potřeba například v případě
// kdy funkce bere jako argument referenci. Ten kdo funkci volá může tedy
// propůjčit svoji mutable referenci pro běh dané funkce a poté co funkce
// doběhne, tak zase může využívat svoji původní mutable referenci.
// Zde v ukázce jde o porušení, protože funkce, které byla reference zapůjčena,
// ji uloží do proměnné `ref`. Poté co funkce doběhne by tak existovala jak
// mutable reference `mref`, tak i reference `ref` což by porušilo půjčovací
// pravidla. Chyba se projeví při dokončování metody `borrow`.

#slide(title: "Example: reborrowing")[
  #codeblock(lineNum: true)[
    ```cpp
    int main() {
      Val<int> v = 5;
      Ref<int> ref;
      RefMut<int> mref = v.ref_mut();
      mref.borrow([](Ref<int> r) {
        ref = r;
        std::println("reborrowed: {}", r);
      });
      std::println("mut: {}", mref);
      std::println("ref: {}", ref);
    }
    ```
  ]
  #stress[Output:]
  ```
  reborrowed: 5
  main.cpp:5:8: borrow violation: Borrowed reference escaped its lifetime.
  ```
]

// V této poslední ukázce je využit slice. Slice je reference na blok paměti
// obsahující více elementů, které v tomhle případě pochází z vektoru. Mutable
// slice je možné rozdělit na více mutable slice pokud každý obsahuje jinou
// část paměti. V první případě je tohle splněno a kód proběhne. V duhém se
// rozsahy překrývají a borlib tedy zahlásí chybu protože by dvě mutable
// reference ukazovali na stejné místo v paměti.

#slide(title: "Example: SliceMut split")[
  #cols(columns: (1fr, 1fr))[
    #codeblock(lineNum: true)[
      ```cpp
      int main() {
        Val<std::vector<int>> vec
          = std::vector{ 3, 1, 4 };
        {
          auto [s1, s2] = vec
            .slice_mut()
            .split({ 0, 1 }, { 1, 3 });
          s1[0] = 1;
          s2[0] = 2;
        }
        std::println("{}", vec[0]);
        std::println("{}", vec[1]);
      }
      ```
    ]
    ```
    1
    2
    ```
  ][
    #codeblock(lineNum: true)[
      ```cpp
      int main() {
        Val<std::vector<int>> vec
          = std::vector{ 3, 1, 4 };
        {
          auto [s1, s2] = vec
            .slice_mut()
            .split({ 0, 2 }, { 1, 3 });
          s1[0] = 2;
          s2[0] = 7;
        }
        std::println("{}", vec[0]);
        std::println("{}", vec[1]);
      }
      ```
    ]
    ```
    main.cpp:7:8: borrow violation: The ranges overlap.
    ```
  ]
]

#slide(title: "Thank you for your attention")[
  #codeblock(lineNum: true)[
    ```cpp
    int main() {
      Ref<int> ref;
      {
        Val<int> n = 5;
        ref = &n;
        std::println("pos1");
      }
      std::println("pos2");
      std::println("pos3: {}", ref);
    }
    ```
  ]
  ```
  pos1
  main.cpp:7:0: borrow violation: Cannot destroy value that is borrowed.
  ```
]

#slide(title: "Sources")[
  Unsound rust example: \
  #link("https://github.com/rust-lang/rust/issues/164250#issuecomment-6460757937")
]