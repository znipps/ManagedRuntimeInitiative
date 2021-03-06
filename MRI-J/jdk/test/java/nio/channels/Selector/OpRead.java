/*
 * Copyright 2002-2004 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 */

/* @test
 * @bug 4755720
 * @summary Test if OP_READ is detected with OP_WRITE in interestOps
 * @library ../../../..
 */

import java.net.*;
import java.io.*;
import java.nio.*;
import java.nio.channels.*;
import java.nio.channels.spi.SelectorProvider;
import java.util.*;

public class OpRead {

    static final int DAYTIME_PORT = 13;
    static final String DAYTIME_HOST = TestEnv.getProperty("host");

    static void test() throws Exception {
        InetSocketAddress isa
            = new InetSocketAddress(InetAddress.getByName(DAYTIME_HOST),
                                    DAYTIME_PORT);
        SocketChannel sc = SocketChannel.open();

        sc.connect(isa);

        sc.configureBlocking(false);

        Selector selector = SelectorProvider.provider().openSelector();
        SelectionKey key = sc.register(selector, SelectionKey.OP_READ |
            SelectionKey.OP_WRITE);

        boolean done = false;
        int failCount = 0;
        while (!done) {
            if (selector.select() > 0) {
                Set keys = selector.selectedKeys();
                Iterator iterator = keys.iterator();
                while (iterator.hasNext()) {
                    key = (SelectionKey)iterator.next();
                    iterator.remove();
                    if (key.isWritable()) {
                        failCount++;
                        if (failCount > 10)
                            throw new RuntimeException("Test failed");
                        Thread.sleep(100);
                    }
                    if (key.isReadable()) {
                        done = true;
                    }
                }
            }
        }


        sc.close();
    }

    public static void main(String[] args) throws Exception {
        test();
    }

}
