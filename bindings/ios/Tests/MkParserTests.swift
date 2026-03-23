import XCTest
@testable import MkParser

final class MkParserTests: XCTestCase {

    func testHeading() throws {
        var opens: [(MkNodeType, MkNodeInfo)] = []
        let p = try MkParser(
            onNodeOpen: { type, node in opens.append((type, node)) }
        )
        try p.feed("# Hello\n").finish()

        let heading = opens.first { $0.0 == .heading }
        XCTAssertNotNil(heading)
        XCTAssertEqual(heading?.1.level, 1)
    }

    func testParagraphText() throws {
        var texts: [String] = []
        let p = try MkParser(onText: { texts.append($0) })
        try p.feed("Hello world\n").finish()
        XCTAssertTrue(texts.joined().contains("Hello"))
    }

    func testFencedCode() throws {
        var opens: [(MkNodeType, MkNodeInfo)] = []
        let p = try MkParser(
            onNodeOpen: { type, node in opens.append((type, node)) }
        )
        try p.feed("```swift\nlet x = 1\n```\n").finish()

        let cb = opens.first { $0.0 == .codeBlock }
        XCTAssertNotNil(cb)
        XCTAssertEqual(cb?.1.lang, "swift")
        XCTAssertEqual(cb?.1.fenced, true)
    }

    func testLink() throws {
        var opens: [(MkNodeType, MkNodeInfo)] = []
        let p = try MkParser(
            onNodeOpen: { type, node in opens.append((type, node)) }
        )
        try p.feed("[click](https://example.com)\n").finish()

        let lk = opens.first { $0.0 == .link }
        XCTAssertNotNil(lk)
        XCTAssertEqual(lk?.1.href, "https://example.com")
    }

    func testStreaming() throws {
        var events = 0
        let p = try MkParser(
            onNodeOpen:  { _, _ in events += 1 },
            onNodeClose: { _, _ in events += 1 }
        )
        let md = "# Title\n\nParagraph.\n"
        for ch in md.utf8 {
            try p.feed(String(bytes: [ch], encoding: .utf8)!)
        }
        try p.finish()
        XCTAssertGreaterThan(events, 0)
    }

    func testDestroyIdempotent() throws {
        let p = try MkParser()
        try p.feed("test\n").finish()
        p.destroy()
        p.destroy()  // second call must be safe
    }
}
